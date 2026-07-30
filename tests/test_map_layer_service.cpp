#include <QtTest>

#include <QDateTime>
#include <QFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUdpSocket>
#include <QUuid>

#include "src/services/MapIntelligenceService.h"
#include "src/services/MapBaseMapService.h"
#include "src/services/MapExternalOverlayService.h"
#include "src/services/MapLayerModel.h"
#include "src/services/MapOperationsService.h"
#include "src/services/MapPskFeedService.h"

namespace {

QByteArray field(const QByteArray& name, const QByteArray& value)
{
    return "<" + name + ":" + QByteArray::number(value.size()) + ">" + value;
}

QByteArray record(const QByteArray& call,
                  const QByteArray& grid,
                  const QByteArray& band,
                  const QByteArray& frequency,
                  const QByteArray& mode,
                  const QByteArray& submode,
                  const QByteArray& confirmationField = {},
                  const QByteArray& confirmationValue = {},
                  const QByteArray& country = {},
                  const QByteArray& continent = {},
                  const QByteArray& cqZone = {},
                  const QByteArray& ituZone = {},
                  const QByteArray& state = {},
                  const QByteArray& iota = {})
{
    QByteArray result = field("CALL", call)
        + field("GRIDSQUARE", grid)
        + field("QSO_DATE", "20260728")
        + field("TIME_ON", call.rightJustified(6, '0').right(6));
    if (!band.isEmpty()) {
        result += field("BAND", band);
    }
    if (!frequency.isEmpty()) {
        result += field("FREQ", frequency);
    }
    result += field("MODE", mode);
    if (!submode.isEmpty()) {
        result += field("SUBMODE", submode);
    }
    if (!confirmationField.isEmpty()) {
        result += field(confirmationField, confirmationValue);
    }
    if (!country.isEmpty()) result += field("COUNTRY", country);
    if (!continent.isEmpty()) result += field("CONT", continent);
    if (!cqZone.isEmpty()) result += field("CQZ", cqZone);
    if (!ituZone.isEmpty()) result += field("ITUZ", ituZone);
    if (!state.isEmpty()) result += field("STATE", state);
    if (!iota.isEmpty()) result += field("IOTA", iota);
    return result + "<EOR>\n";
}

bool databaseHasIndex(const QString& databasePath, const QString& indexName)
{
    QString const connectionName =
        QStringLiteral("map_test_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    bool found = false;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName);
        database.setDatabaseName(databasePath);
        if (database.open()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name=:name"));
            query.bindValue(QStringLiteral(":name"), indexName);
            found = query.exec() && query.next() && query.value(0).toInt() == 1;
        }
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return found;
}

} // namespace

class TestMapIntelligenceService final : public QObject
{
    Q_OBJECT

private slots:
    void persistsIndexesFiltersAndLiveRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("logbook.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("map-intelligence.sqlite"));
        QFile iotaCatalog(tempDir.filePath(QStringLiteral("iota_groups.json")));
        QVERIFY(iotaCatalog.open(QIODevice::WriteOnly));
        iotaCatalog.write(R"json([
            {
                "refno": "EU-005",
                "name": "Great Britain",
                "dxcc_num": "223",
                "latitude_max": "59.00",
                "latitude_min": "49.75",
                "longitude_max": "2.00",
                "longitude_min": "-8.25",
                "pc_credited": "41.2",
                "comment": ""
            },
            {
                "refno": "OC-001",
                "name": "Australia",
                "dxcc_num": "150",
                "latitude_max": "-10.00",
                "latitude_min": "-39.25",
                "longitude_max": "153.75",
                "longitude_min": "113.00",
                "pc_credited": "18.0",
                "comment": ""
            }
        ])json");
        iotaCatalog.close();
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("TEST1", "FN20aa", "20M", "", "FT8", "", {}, {},
                          "United States", "NA", "5", "8", "PA", "EU-005"));
        file.write(record("TEST2", "FN20bb", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y",
                          "United States", "NA", "5", "8", "NJ"));
        file.write(record("TEST3", "JN70", "40m", "", "MFSK", "FT4",
                          "QSL_RCVD", "Y",
                          "Italy", "EU", "15", "28"));
        file.write(record("TEST4", "JO21", "", "14.074", "MFSK", "FT4",
                          {}, {}, "Netherlands", "EU", "14", "27"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        QVERIFY(!service.coveragePushPinsEnabled());
        QVERIFY(!service.timeZoneOverlayEnabled());
        service.setCoveragePushPinsEnabled(true);
        service.setTimeZoneOverlayEnabled(true);
        service.reloadFromAdif(adifPath);
        QTRY_VERIFY_WITH_TIMEOUT(!service.loading(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 4, 5000);

        auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
        QVERIFY(layerModel);
        auto* baseMap = qobject_cast<MapBaseMapService*>(service.baseMapService());
        QVERIFY(baseMap);
        auto* externalOverlays =
            qobject_cast<MapExternalOverlayService*>(service.externalOverlayService());
        QVERIFY(externalOverlays);
        auto* pskFeed = qobject_cast<MapPskFeedService*>(service.pskFeedService());
        QVERIFY(pskFeed);
        QVERIFY(!layerModel->layerEnabled(QStringLiteral("offline")));
        QVERIFY(!baseMap->offlineMode());
        QVERIFY(!baseMap->baseMapImage().isNull());

        layerModel->setLayerEnabled(QStringLiteral("offline"), true);
        QTRY_VERIFY_WITH_TIMEOUT(baseMap->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(externalOverlays->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(pskFeed->offlineMode(), 1000);
        QCOMPARE(baseMap->attribution(), QStringLiteral("Decodium Atlas (local)"));
        baseMap->setProvider(QStringLiteral("NASA GIBS satellite"));
        QCOMPARE(baseMap->provider(), QStringLiteral("NASA GIBS satellite"));
        QVERIFY(baseMap->offlineMode());
        QCOMPARE(baseMap->attribution(), QStringLiteral("Decodium Atlas (local)"));
        baseMap->setProvider(QStringLiteral("Decodium Atlas"));

        layerModel->setLayerEnabled(QStringLiteral("offline"), false);
        QTRY_VERIFY_WITH_TIMEOUT(!baseMap->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!externalOverlays->offlineMode(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(!pskFeed->offlineMode(), 1000);
        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 4, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookRows().size(), 4, 5000);
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Mercator")));
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Miller")));
        QVERIFY(operations->availableProjections().contains(
            QStringLiteral("Azimuthal Equidistant")));
        QCOMPARE(operations->dataViewMode(), QStringLiteral("Live + Logbook"));

        operations->setMapProjection(QStringLiteral("Mercator"));
        QCOMPARE(operations->mapProjection(), QStringLiteral("Mercator"));
        operations->setDataViewMode(QStringLiteral("Logbook"));
        QCOMPARE(operations->dataViewMode(), QStringLiteral("Logbook"));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("live")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("worked")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("confirmed")));
        operations->setDataViewMode(QStringLiteral("Live + Logbook"));

        MapIntelligenceService persistedService(nullptr, databasePath);
        QVERIFY(persistedService.coveragePushPinsEnabled());
        QVERIFY(persistedService.timeZoneOverlayEnabled());

        operations->setLogbookSearch(QStringLiteral("TEST2"));
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 1, 5000);
        QCOMPARE(operations->logbookRows().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("TEST2"));
        operations->setLogbookSearch(QString());
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 4, 5000);

        QString const csvPath = tempDir.filePath(QStringLiteral("filtered.csv"));
        QVERIFY(operations->exportLogbook(csvPath, QStringLiteral("CSV")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QByteArray const csvData = csv.readAll();
        QVERIFY(csvData.startsWith("Date,Time,Call,Grid"));
        QVERIFY(csvData.contains("\"TEST2\""));
        csv.close();

        QString const adifExport =
            tempDir.filePath(QStringLiteral("filtered.adi"));
        QVERIFY(operations->exportLogbook(adifExport, QStringLiteral("ADIF")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile adif(adifExport);
        QVERIFY(adif.open(QIODevice::ReadOnly));
        QByteArray const adifData = adif.readAll();
        QVERIFY(adifData.contains("<ADIF_VER:5>3.1.4"));
        QVERIFY(adifData.contains("<CALL:5>TEST2"));
        adif.close();

        operations->saveMapPreset(QStringLiteral("Unit Test"));
        QVERIFY(operations->mapPresets().contains(QStringLiteral("Unit Test")));
        operations->setMapProjection(QStringLiteral("Miller"));
        operations->applyMapPreset(QStringLiteral("Unit Test"));
        QCOMPARE(operations->mapProjection(), QStringLiteral("Mercator"));
        operations->deleteMapPreset(QStringLiteral("Unit Test"));
        QVERIFY(!operations->mapPresets().contains(QStringLiteral("Unit Test")));

        QUdpSocket rotatorReceiver;
        QVERIFY(rotatorReceiver.bind(QHostAddress::LocalHost, 0));
        operations->setRotatorHost(QStringLiteral("127.0.0.1"));
        operations->setRotatorPort(rotatorReceiver.localPort());
        operations->setRotatorEnabled(true);
        operations->aimRotator(123.6);
        QTRY_VERIFY_WITH_TIMEOUT(rotatorReceiver.hasPendingDatagrams(), 2000);
        QByteArray rotatorPayload;
        rotatorPayload.resize(
            static_cast<int>(rotatorReceiver.pendingDatagramSize()));
        rotatorReceiver.readDatagram(rotatorPayload.data(),
                                     rotatorPayload.size());
        QCOMPARE(rotatorPayload,
                 QByteArray("<PST><AZIMUTH>124</AZIMUTH></PST>"));

        QCOMPARE(service.workedGridCount(), 3);
        QCOMPARE(service.confirmedGridCount(), 2);
        QVERIFY(service.availableBands().contains(QStringLiteral("20m")));
        QVERIFY(service.availableBands().contains(QStringLiteral("40m")));
        QVERIFY(service.availableModes().contains(QStringLiteral("FT8")));
        QVERIFY(service.availableModes().contains(QStringLiteral("FT4")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_qso_band_mode")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_spot_observed")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_qso_grid6_status")));
        QVERIFY(databaseHasIndex(databasePath, QStringLiteral("idx_map_spot_activity_time")));

        QCOMPARE(layerModel->rowCount(), 23);
        auto hasLayer = [layerModel](QString const& id) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                if (layerModel->data(
                        layerModel->index(row, 0),
                        MapLayerModel::LayerIdRole).toString() == id) {
                    return true;
                }
            }
            return false;
        };
        for (QString const& layerId : {
                 QStringLiteral("pota"), QStringLiteral("states"),
                 QStringLiteral("counties"), QStringLiteral("iota"),
                 QStringLiteral("wpx"), QStringLiteral("earthquakes"),
                 QStringLiteral("wildfires")}) {
            QVERIFY2(hasLayer(layerId),
                     qPrintable(QStringLiteral("Missing map layer: %1")
                                    .arg(layerId)));
        }
        QVERIFY(layerModel->layerEnabled(QStringLiteral("active")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("missing")));
        QVERIFY(layerModel->layerEnabled(QStringLiteral("psk")));
        QVERIFY(!service.awards().isEmpty());
        QVERIFY(service.availableContinents().contains(QStringLiteral("EU")));
        QVERIFY(service.availableDxcc().contains(QStringLiteral("Italy")));
        QVERIFY(service.availableSources().contains(QStringLiteral("ADIF")));
        QCOMPARE(service.gridPrecision(), 4);
        QCOMPARE(service.liveDecayMinutes(), 15);
        QVERIFY(service.splitGridEnabled());
        QCOMPARE(service.pskDisplayMode(), QStringLiteral("Overlay"));
        QCOMPARE(service.pskOpacityPercent(), 65);
        QCOMPARE(service.callLookupProvider(), QStringLiteral("QRZ"));
        QVERIFY(service.alertNewGridEnabled());
        QVERIFY(service.alertNewDxccEnabled());
        QVERIFY(service.alertCqEnabled());
        QVERIFY(!service.statistics().isEmpty());
        QCOMPARE(service.statistics().value(QStringLiteral("qso")).toInt(), 4);
        QCOMPARE(service.statistics().value(QStringLiteral("confirmed")).toInt(), 2);
        layerModel->setLayerEnabled(QStringLiteral("iota"), true);
        auto layerCount = [layerModel](QString const& id) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                QModelIndex const index = layerModel->index(row, 0);
                if (layerModel->data(index, MapLayerModel::LayerIdRole).toString()
                    == id) {
                    return layerModel->data(index, MapLayerModel::CountRole).toInt();
                }
            }
            return -1;
        };
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("iota")), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->operationalMarkers().size(), 2, 5000);
        bool hasIotaMarker = false;
        bool hasCatalogOnlyMarker = false;
        for (QVariant const& markerValue : operations->operationalMarkers()) {
            QVariantMap const marker = markerValue.toMap();
            if (marker.value(QStringLiteral("type")).toString()
                == QStringLiteral("IOTA")
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("EU-005")) {
                QVERIFY(marker.value(QStringLiteral("worked")).toBool());
                hasIotaMarker = true;
            }
            if (marker.value(QStringLiteral("type")).toString()
                    == QStringLiteral("IOTA")
                && marker.value(QStringLiteral("reference")).toString()
                       == QStringLiteral("OC-001")) {
                QVERIFY(marker.value(QStringLiteral("catalog")).toBool());
                QVERIFY(!marker.value(QStringLiteral("worked")).toBool());
                hasCatalogOnlyMarker = true;
            }
        }
        QVERIFY(hasIotaMarker);
        QVERIFY(hasCatalogOnlyMarker);

        service.setGridPrecision(6);
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 4, 5000);
        service.setGridPrecision(4);
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 3, 5000);

        service.setBandFilter(QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 2, 5000);
        QCOMPARE(service.confirmedGridCount(), 1);

        service.setModeFilter(QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 1, 5000);
        QCOMPARE(service.confirmedGridCount(), 1);

        service.setWorkedLayerEnabled(false);
        service.setConfirmedLayerEnabled(true);
        QCOMPARE(service.coverageCells().size(), 1);
        QVERIFY(service.coverageCells().first().toMap()
                    .value(QStringLiteral("confirmed")).toBool());

        service.setBandFilter(QStringLiteral("All"));
        service.setModeFilter(QStringLiteral("All"));
        service.setContinentFilter(QStringLiteral("EU"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 2, 5000);
        service.setContinentFilter(QStringLiteral("All"));
        service.setDxccFilter(QStringLiteral("Italy"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 1, 5000);
        service.setDxccFilter(QStringLiteral("All"));
        service.setWorkedLayerEnabled(true);
        service.appendAdifRecord(record("TEST5", "KM71", "20m", "", "FT8", ""));
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 5, 5000);
        QCOMPARE(service.workedGridCount(), 4);
        QCOMPARE(service.confirmedGridCount(), 2);

        QVariantMap decode;
        decode.insert(QStringLiteral("time"), QStringLiteral("120001"));
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("CQ LIVE1 JN70"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("LIVE1"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("JN70"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), QStringLiteral("-17"));
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("Italy"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        decode.insert(QStringLiteral("cqZone"), 15);
        decode.insert(QStringLiteral("ituZone"), 28);
        decode.insert(QStringLiteral("isCQ"), true);
        decode.insert(QStringLiteral("distanceKm"), 1234.0);
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QCOMPARE(service.activeGridCount(), 1);
        QCOMPARE(service.missingGridCount(), 0);
        QCOMPARE(service.roster().size(), 1);
        QVariantMap const liveRow = service.roster().first().toMap();
        QCOMPARE(liveRow.value(QStringLiteral("call")).toString(), QStringLiteral("LIVE1"));
        QCOMPARE(liveRow.value(QStringLiteral("grid")).toString(), QStringLiteral("JN70"));
        QCOMPARE(liveRow.value(QStringLiteral("frequencyHz")).toLongLong(), 14075500);
        QCOMPARE(liveRow.value(QStringLiteral("dxcc")).toString(), QStringLiteral("Italy"));
        QCOMPARE(liveRow.value(QStringLiteral("distanceKm")).toDouble(), 1234.0);
        bool hasLiveOpacity = false;
        for (QVariant const& cellValue : service.coverageCells()) {
            QVariantMap const cell = cellValue.toMap();
            if (cell.value(QStringLiteral("grid")).toString() == QStringLiteral("JN70")) {
                hasLiveOpacity =
                    cell.value(QStringLiteral("liveOpacity")).toDouble() > 0.0;
                QCOMPARE(cell.value(QStringLiteral("liveStatus")).toString(),
                         QStringLiteral("CQ"));
            }
        }
        QVERIFY(hasLiveOpacity);

        QVERIFY(service.availableSources().contains(QStringLiteral("ADIF")));
        QVERIFY(service.availableSources().contains(QStringLiteral("decoder")));

        service.setSourceFilter(QStringLiteral("ADIF"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        QCOMPARE(service.activeGridCount(), 0);
        QCOMPARE(service.workedGridCount(), 4);

        service.setSourceFilter(QStringLiteral("decoder"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 0, 5000);
        QCOMPARE(service.liveSpotCount(), 1);
        QCOMPARE(service.activeGridCount(), 1);

        service.setBandFilter(QStringLiteral("40m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        service.setBandFilter(QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setModeFilter(QStringLiteral("FT4"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        service.setModeFilter(QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setBandFilter(QStringLiteral("All"));
        service.setModeFilter(QStringLiteral("All"));
        service.setSourceFilter(QStringLiteral("All"));
        QTRY_COMPARE_WITH_TIMEOUT(service.workedGridCount(), 4, 5000);
        QCOMPARE(service.liveSpotCount(), 1);

        // Active-grid coverage must remain drawable when the generic live
        // marker layer is hidden. The two controls represent independent
        // visual layers in the map UI.
        service.setLiveLayerEnabled(false);
        service.setActiveLayerEnabled(true);
        QVariantMap activeCoverage;
        for (QVariant const& cellValue : service.coverageCells()) {
            QVariantMap const cell = cellValue.toMap();
            if (cell.value(QStringLiteral("grid")).toString() == QStringLiteral("JN70")) {
                activeCoverage = cell;
                break;
            }
        }
        QVERIFY(!activeCoverage.isEmpty());
        QVERIFY(activeCoverage.value(QStringLiteral("active")).toBool());
        service.setLiveLayerEnabled(true);

        service.setCqOnly(true);
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setRosterSort(QStringLiteral("Distance"));
        service.setRosterSortDescending(false);
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        service.setRosterTextMode(QStringLiteral("Only"));
        service.setRosterTextFilter(QStringLiteral("LIVE1"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        service.setRosterTextFilter(QStringLiteral("NO-MATCH"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 0, 5000);
        service.setRosterTextMode(QStringLiteral("No filter"));
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);

        QVariantMap psk;
        psk.insert(QStringLiteral("call"), QStringLiteral("PSK1"));
        psk.insert(QStringLiteral("grid"), QStringLiteral("KM72"));
        psk.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        psk.insert(QStringLiteral("frequency"), 14076000);
        psk.insert(QStringLiteral("snr"), -21);
        psk.insert(QStringLiteral("country"), QStringLiteral("Israel"));
        psk.insert(QStringLiteral("continent"), QStringLiteral("AS"));
        psk.insert(QStringLiteral("cqZone"), 20);
        psk.insert(QStringLiteral("ituZone"), 39);
        psk.insert(QStringLiteral("distanceKm"), 2100.0);
        service.ingestPskSpots({psk}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        service.setCqOnly(false);
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QVERIFY(service.availableSources().contains(QStringLiteral("psk")));
        QCOMPARE(service.activeGridCount(), 2);
        QCOMPARE(service.missingGridCount(), 1);
        QVERIFY(service.unreadAlertCount() > 0);
        QVERIFY(!service.alerts().isEmpty());
        service.setPskOpacityPercent(40);
        service.setPskDisplayMode(QStringLiteral("Replace"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.activeGridCount(), 1, 5000);
        QVariantMap pskCoverage;
        for (QVariant const& value : service.coverageCells()) {
            QVariantMap const cell = value.toMap();
            if (cell.value(QStringLiteral("grid")).toString()
                == QStringLiteral("KM72")) {
                pskCoverage = cell;
                break;
            }
        }
        QVERIFY(!pskCoverage.isEmpty());
        QCOMPARE(pskCoverage.value(QStringLiteral("liveStatus")).toString(),
                 QStringLiteral("PSK"));
        QVERIFY(pskCoverage.value(QStringLiteral("liveOpacity")).toDouble() <= 0.4);
        service.setPskDisplayMode(QStringLiteral("Overlay"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        service.setCallLookupProvider(QStringLiteral("HamQTH"));
        QCOMPARE(service.callLookupProvider(), QStringLiteral("HamQTH"));

        service.selectGrid(QStringLiteral("JN70"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGrid(), QStringLiteral("JN70"));
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("workedCount")).toInt(), 1);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("confirmedCount")).toInt(), 1);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("activeCount")).toInt(), 1);
        QCOMPARE(service.selectedGridLive().size(), 1);
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("LIVE1"));
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("activityType")).toString(),
                 QStringLiteral("CQ"));
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("gridEvidence")).toString(),
                 QStringLiteral("TX locator in decoded message"));
        QCOMPARE(service.selectedGridQsos().size(), 1);
        QCOMPARE(service.selectedGridQsos().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("TEST3"));

        QVariantMap repeatedGridDecode;
        repeatedGridDecode.insert(QStringLiteral("time"), QStringLiteral("120002"));
        repeatedGridDecode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        repeatedGridDecode.insert(QStringLiteral("message"),
                                  QStringLiteral("R1EMOTE TESTER KN37"));
        repeatedGridDecode.insert(QStringLiteral("fromCall"), QStringLiteral("R1EMOTE"));
        repeatedGridDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("KN37"));
        repeatedGridDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        repeatedGridDecode.insert(QStringLiteral("db"), QStringLiteral("-10"));
        repeatedGridDecode.insert(QStringLiteral("freq"), 1600);
        service.ingestDecodeEntry(repeatedGridDecode, 14074000, QStringLiteral("20m"));
        repeatedGridDecode.insert(QStringLiteral("time"), QStringLiteral("120003"));
        repeatedGridDecode.insert(QStringLiteral("timestamp"),
                                  QDateTime::currentMSecsSinceEpoch() + 1000);
        service.ingestDecodeEntry(repeatedGridDecode, 14074000, QStringLiteral("20m"));

        QVariantMap staleGridDecode;
        staleGridDecode.insert(QStringLiteral("time"), QStringLiteral("120004"));
        staleGridDecode.insert(QStringLiteral("timestamp"),
                               QDateTime::currentMSecsSinceEpoch() + 2000);
        staleGridDecode.insert(QStringLiteral("message"),
                               QStringLiteral("N0GRID TESTER -10"));
        staleGridDecode.insert(QStringLiteral("fromCall"), QStringLiteral("N0GRID"));
        staleGridDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("KN37"));
        staleGridDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        staleGridDecode.insert(QStringLiteral("db"), QStringLiteral("-10"));
        staleGridDecode.insert(QStringLiteral("freq"), 1700);
        service.ingestDecodeEntry(staleGridDecode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 5, 5000);
        service.selectGrid(QStringLiteral("KN37"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.gridDetailsLoading(), 5000);
        QCOMPARE(service.selectedGridSummary()
                     .value(QStringLiteral("activeCount")).toInt(), 1);
        QCOMPARE(service.selectedGridLive().size(), 1);
        QCOMPARE(service.selectedGridLive().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("R1EMOTE"));

        service.markAlertsRead();
        QTRY_COMPARE_WITH_TIMEOUT(service.unreadAlertCount(), 0, 5000);
        service.clearAlerts();
        QTRY_VERIFY_WITH_TIMEOUT(service.alerts().isEmpty(), 5000);

        service.setAlertNewGridEnabled(false);
        service.setAlertNewDxccEnabled(false);
        service.setAlertCqEnabled(false);
        service.setAlertCallPattern(QStringLiteral("WATCH*"));
        decode.insert(QStringLiteral("timestamp"),
                      QDateTime::currentMSecsSinceEpoch() + 1000);
        decode.insert(QStringLiteral("message"), QStringLiteral("WATCHME LIVE"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("WATCHME"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("IO91"));
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.unreadAlertCount(), 1, 5000);
        QCOMPARE(service.alerts().first().toMap()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("call_watch"));

        service.clearLiveSpots();
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 0, 5000);
        QVERIFY(service.roster().isEmpty());
    }

    void attributesDirectedDecodeToTransmittingStation()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const databasePath =
            tempDir.filePath(QStringLiteral("directed-message-map.sqlite"));
        MapIntelligenceService service(nullptr, databasePath);

        QVariantMap decode;
        decode.insert(QStringLiteral("time"), QStringLiteral("075255"));
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("WA1BXY UA3GIE KO92"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("WA1BXY"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KO92"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), QStringLiteral("-11"));
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("United States"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("NA"));
        decode.insert(QStringLiteral("cqZone"), 5);
        decode.insert(QStringLiteral("ituZone"), 8);

        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.roster().size(), 1, 5000);
        QVariantMap const row = service.roster().first().toMap();
        QCOMPARE(row.value(QStringLiteral("call")).toString(),
                 QStringLiteral("UA3GIE"));
        QCOMPARE(row.value(QStringLiteral("targetCall")).toString(),
                 QStringLiteral("WA1BXY"));
        QCOMPARE(row.value(QStringLiteral("grid")).toString(),
                 QStringLiteral("KO92"));
        QVERIFY(row.value(QStringLiteral("dxcc")).toString()
                    != QStringLiteral("United States"));
        QVERIFY(row.value(QStringLiteral("continent")).toString()
                    != QStringLiteral("NA"));
    }

    void repairsPersistedDirectedDecodeAttribution()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
        QString const databasePath =
            tempDir.filePath(QStringLiteral("persisted-attribution-map.sqlite"));

        {
            MapIntelligenceService service(nullptr, databasePath);
            QVariantMap decode;
            decode.insert(QStringLiteral("time"), QStringLiteral("075255"));
            decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
            decode.insert(QStringLiteral("message"), QStringLiteral("WA1BXY UA3GIE KO92"));
            decode.insert(QStringLiteral("fromCall"), QStringLiteral("WA1BXY"));
            decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KO92"));
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), QStringLiteral("-11"));
            decode.insert(QStringLiteral("freq"), 1500);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
            QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        }

        QString const connectionName =
            QStringLiteral("map_attribution_test_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "UPDATE map_spot SET call='WA1BXY', target_call='',"
                " dxcc='United States', continent='NA', cq_zone=5, itu_zone=8,"
                " state='RI'")));
            QVERIFY(query.exec(QStringLiteral(
                "UPDATE map_spot_event SET call='WA1BXY'")));
            QVERIFY(query.exec(QStringLiteral(
                "DELETE FROM map_meta"
                " WHERE key='decoder_sender_attribution_version'")));
            database.close();
        }
        QSqlDatabase::removeDatabase(connectionName);

        {
            MapIntelligenceService service(nullptr, databasePath);
            QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        }

        QString const verifyConnectionName =
            QStringLiteral("map_attribution_verify_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        {
            QSqlDatabase database =
                QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                          verifyConnectionName);
            database.setDatabaseName(databasePath);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec(QStringLiteral(
                "SELECT call, target_call, dxcc, continent, state FROM map_spot")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("UA3GIE"));
            QCOMPARE(query.value(1).toString(), QStringLiteral("WA1BXY"));
            QVERIFY(query.value(2).toString() != QStringLiteral("United States"));
            QVERIFY(query.value(3).toString() != QStringLiteral("NA"));
            QVERIFY(query.value(4).toString().isEmpty());
            QVERIFY(query.exec(QStringLiteral("SELECT call FROM map_spot_event")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("UA3GIE"));
            database.close();
        }
        QSqlDatabase::removeDatabase(verifyConnectionName);
    }

    void exportsAllFilteredRowsBeyondVisibleLimit()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("large-logbook.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("large-map-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        for (int index = 0; index < 75; ++index) {
            QByteArray const call =
                QByteArray("EX") + QByteArray::number(index).rightJustified(4, '0');
            file.write(record(call, "JN70", "20m", "", "FT8", ""));
        }
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_VERIFY_WITH_TIMEOUT(!service.loading(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 75, 5000);

        auto* operations =
            qobject_cast<MapOperationsService*>(service.operationsService());
        QVERIFY(operations);
        operations->setLogbookLimit(50);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookTotal(), 75, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations->logbookRows().size(), 50, 5000);

        QString const csvPath = tempDir.filePath(QStringLiteral("all-filtered.csv"));
        QVERIFY(operations->exportLogbook(csvPath, QStringLiteral("CSV")));
        QTRY_VERIFY_WITH_TIMEOUT(!operations->exportInProgress(), 5000);
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QByteArray const data = csv.readAll();
        QCOMPARE(data.count('\n'), 76);
        QVERIFY(data.contains("\"EX0000\""));
        QVERIFY(data.contains("\"EX0074\""));
    }

    void buildsIndependentOperationalRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("roster.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("roster-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("DONE1", "FN20", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y", "United States", "NA",
                          {}, {}, "PA"));
        file.write(record("PENDING1", "JN70", "20m", "", "FT8", "",
                          {}, {}, "Italy", "EU"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);
        QVERIFY(service.availableAwardPrograms().contains(QStringLiteral("WAC")));
        QVERIFY(service.availableAwardPrograms().contains(QStringLiteral("US48")));
        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QCOMPARE(awardByLabel(QStringLiteral("WAC"))
                     .value(QStringLiteral("worked")).toInt(), 2);
        QCOMPARE(awardByLabel(QStringLiteral("US48"))
                     .value(QStringLiteral("worked")).toInt(), 1);

        auto ingest = [&service](QString const& call,
                                 QString const& grid,
                                 QString const& dxcc,
                                 QString const& continent,
                                 int snr,
                                 qint64 timestamp) {
            QVariantMap decode;
            decode.insert(QStringLiteral("time"), QString::number(timestamp));
            decode.insert(QStringLiteral("timestamp"), timestamp);
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), snr);
            decode.insert(QStringLiteral("freq"), 1500);
            decode.insert(QStringLiteral("dxcc"), dxcc);
            decode.insert(QStringLiteral("continent"), continent);
            decode.insert(QStringLiteral("isCQ"), true);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        };

        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        ingest(QStringLiteral("DONE1"), QStringLiteral("FN20"),
               QStringLiteral("United States"), QStringLiteral("NA"), -12, now);
        ingest(QStringLiteral("PENDING1"), QStringLiteral("JN70"),
               QStringLiteral("Italy"), QStringLiteral("EU"), -15, now + 1);
        ingest(QStringLiteral("NEW1"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), -18, now + 2);

        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        QCOMPARE(service.rosterNewCount(), 1);
        QCOMPARE(service.rosterUnconfirmedCount(), 1);
        QCOMPARE(service.rosterWantedCount(), 2);

        auto findCall = [&service](QString const& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString() == call) return row;
            }
            return QVariantMap {};
        };
        QCOMPARE(findCall(QStringLiteral("DONE1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("CONFIRMED"));
        QCOMPARE(findCall(QStringLiteral("PENDING1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("UNCONFIRMED"));
        QCOMPARE(findCall(QStringLiteral("NEW1")).value(QStringLiteral("status")).toString(),
                 QStringLiteral("NEW"));

        // A newer decode replaces the previous row for the same station.
        ingest(QStringLiteral("NEW1"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), -5, now + 3);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            findCall(QStringLiteral("NEW1")).value(QStringLiteral("snr")).toInt(), -5, 5000);

        service.setRosterStatusFilter(QStringLiteral("New"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));
        service.setRosterStatusFilter(QStringLiteral("Unconfirmed"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.roster().first().toMap().value(QStringLiteral("call")).toString(),
            QStringLiteral("PENDING1"), 5000);
        QCOMPARE(service.rosterCount(), 1);
        service.setRosterStatusFilter(QStringLiteral("Wanted"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        service.setRosterStatusFilter(QStringLiteral("All"));

        // Map filters must not hide active stations from the operational roster.
        service.setContinentFilter(QStringLiteral("EU"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QCOMPARE(service.rosterCount(), 3);

        QVariantMap psk;
        psk.insert(QStringLiteral("call"), QStringLiteral("PSK-RX"));
        psk.insert(QStringLiteral("grid"), QStringLiteral("JO21"));
        psk.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        psk.insert(QStringLiteral("freq"), 14076000);
        psk.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        service.ingestPskSpots({psk}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QCOMPARE(service.rosterCount(), 3);

        service.setRosterCallWatched(QStringLiteral("NEW1"), true);
        QTRY_VERIFY_WITH_TIMEOUT(
            findCall(QStringLiteral("NEW1")).value(QStringLiteral("watched")).toBool(),
            5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));
        service.setRosterStatusFilter(QStringLiteral("Watched"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap().value(QStringLiteral("call")).toString(),
                 QStringLiteral("NEW1"));

        service.setRosterStatusFilter(QStringLiteral("All"));
        service.setRosterCallIgnored(QStringLiteral("NEW1"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        QVERIFY(findCall(QStringLiteral("NEW1")).isEmpty());

        MapIntelligenceService persisted(nullptr, databasePath);
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 2, 5000);
        service.clearRosterPreferences();
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 3, 5000);
        persisted.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 3, 5000);
    }

    void keepsAllTimeAwardProgressWithTemporaryMapPeriod()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("historical.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("historical-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(field("CALL", "OLD1")
                   + field("GRIDSQUARE", "JN70")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260101")
                   + field("TIME_ON", "120000")
                   + field("COUNTRY", "Italy")
                   + field("CONT", "EU")
                   + "<EOR>\n");
        file.write(field("CALL", "OLD2")
                   + field("GRIDSQUARE", "KM18")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260102")
                   + field("TIME_ON", "120000")
                   + field("COUNTRY", "Greece")
                   + field("CONT", "EU")
                   + field("LOTW_QSL_RCVD", "Y")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);
        QCOMPARE(service.statistics().value(QStringLiteral("qso")).toInt(), 2);
        QCOMPARE(service.statistics().value(QStringLiteral("totalQso")).toInt(), 2);

        service.setPeriodFilter(QStringLiteral("7 days"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.statistics().value(QStringLiteral("qso")).toInt(), 0, 5000);
        QCOMPARE(service.statistics().value(QStringLiteral("totalQso")).toInt(), 2);
        QCOMPARE(service.statistics().value(QStringLiteral("totalConfirmed")).toInt(), 1);
        QCOMPARE(service.statistics().value(QStringLiteral("period")).toString(),
                 QStringLiteral("7 days"));

        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("DXCC"))
                .value(QStringLiteral("worked")).toInt(),
            2, 5000);
        QCOMPARE(awardByLabel(QStringLiteral("DXCC"))
                     .value(QStringLiteral("confirmed")).toInt(),
                 1);
        QVERIFY(awardByLabel(QStringLiteral("DXCC"))
                    .value(QStringLiteral("scope")).toString()
                    .endsWith(QStringLiteral("All time")));

        service.setPeriodFilter(QStringLiteral("All time"));
        QTRY_COMPARE_WITH_TIMEOUT(
            service.statistics().value(QStringLiteral("qso")).toInt(), 2, 5000);
    }

    void enrichesSparseDecodiumAdifForRosterAndAwards()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("decodium-log.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("decodium-map-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        // Decodium's normal ADI records intentionally omit COUNTRY, CONT,
        // CQZ and ITUZ.  The map must derive those values from cty.dat.
        file.write(field("CALL", "SV1ABC")
                   + field("GRIDSQUARE", "KM18")
                   + field("BAND", "20m")
                   + field("MODE", "FT8")
                   + field("QSO_DATE", "20260728")
                   + field("TIME_ON", "120000")
                   + "<EOR>\n");
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.availableDxcc().contains(
                                      QStringLiteral("Greece"), Qt::CaseInsensitive),
                                  5000);

        auto awardByLabel = [&service](QString const& label) {
            for (QVariant const& value : service.awards()) {
                QVariantMap const award = value.toMap();
                if (award.value(QStringLiteral("label")).toString() == label) {
                    return award;
                }
            }
            return QVariantMap {};
        };
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("DXCC")).value(QStringLiteral("worked")).toInt(),
            1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            awardByLabel(QStringLiteral("Maidenhead")).value(QStringLiteral("worked")).toInt(),
            1, 5000);

        QVariantMap decode;
        decode.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());
        decode.insert(QStringLiteral("message"), QStringLiteral("CQ SV2TEST KM18"));
        decode.insert(QStringLiteral("fromCall"), QStringLiteral("SV2TEST"));
        decode.insert(QStringLiteral("dxGrid"), QStringLiteral("KM18"));
        decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decode.insert(QStringLiteral("db"), -8);
        decode.insert(QStringLiteral("freq"), 1500);
        decode.insert(QStringLiteral("dxcc"), QStringLiteral("Greece"));
        decode.insert(QStringLiteral("continent"), QStringLiteral("EU"));
        decode.insert(QStringLiteral("isCQ"), true);
        service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));

        auto findRoster = [&service](QString const& call) {
            for (QVariant const& value : service.roster()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("call")).toString() == call) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QTRY_VERIFY_WITH_TIMEOUT(!findRoster(QStringLiteral("SV2TEST")).isEmpty(), 5000);
        QVariantMap const rosterRow = findRoster(QStringLiteral("SV2TEST"));
        QVERIFY(rosterRow.value(QStringLiteral("dxccWorked")).toBool());
        QVERIFY(!rosterRow.value(QStringLiteral("huntReason")).toString()
                     .contains(QStringLiteral("New DXCC"), Qt::CaseInsensitive));
    }

    void appliesAwardRulesAndDetailedIgnoreLists()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        QString const adifPath = tempDir.filePath(QStringLiteral("awards.adi"));
        QString const databasePath =
            tempDir.filePath(QStringLiteral("awards-intelligence.sqlite"));
        QFile file(adifPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Decodium ADIF\n<EOH>\n");
        file.write(record("USWORK", "FN20", "20m", "", "FT8", "",
                          "LOTW_QSL_RCVD", "Y",
                          "United States", "NA", "5", "8", "PA"));
        file.write(record("ITWORK", "JN70", "20m", "", "FT8", "",
                          {}, {}, "Italy", "EU", "15", "28"));
        file.close();

        MapIntelligenceService service(nullptr, databasePath);
        service.reloadFromAdif(adifPath);
        QTRY_COMPARE_WITH_TIMEOUT(service.qsoCount(), 2, 5000);

        auto ingest = [&service](QString const& call,
                                 QString const& grid,
                                 QString const& dxcc,
                                 QString const& continent,
                                 int cqZone,
                                 int ituZone,
                                 QString const& state) {
            QVariantMap decode;
            decode.insert(QStringLiteral("timestamp"),
                          QDateTime::currentMSecsSinceEpoch());
            decode.insert(QStringLiteral("time"), QStringLiteral("120000"));
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), -12);
            decode.insert(QStringLiteral("freq"), 1500);
            decode.insert(QStringLiteral("dxcc"), dxcc);
            decode.insert(QStringLiteral("continent"), continent);
            decode.insert(QStringLiteral("cqZone"), cqZone);
            decode.insert(QStringLiteral("ituZone"), ituZone);
            decode.insert(QStringLiteral("state"), state);
            decode.insert(QStringLiteral("isCQ"), true);
            service.ingestDecodeEntry(decode, 14074000, QStringLiteral("20m"));
        };

        ingest(QStringLiteral("LIVEIT"), QStringLiteral("JN70"),
               QStringLiteral("Italy"), QStringLiteral("EU"), 15, 28, {});
        ingest(QStringLiteral("LIVEIL"), QStringLiteral("KM72"),
               QStringLiteral("Israel"), QStringLiteral("AS"), 20, 39, {});
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);

        service.setActiveAwardProgram(QStringLiteral("WAZ"));
        service.setAwardGoal(QStringLiteral("Confirmed"));
        service.setRosterStatusFilter(QStringLiteral("Award"));
        QTRY_VERIFY_WITH_TIMEOUT(
            !service.roster().isEmpty()
                && service.roster().first().toMap()
                       .value(QStringLiteral("awardProgram")).toString()
                       == QStringLiteral("WAZ")
                && service.roster().first().toMap()
                       .value(QStringLiteral("awardWanted")).toBool(),
            5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 2, 5000);
        for (QVariant const& value : service.roster()) {
            QVERIFY(value.toMap().value(QStringLiteral("awardWanted")).toBool());
            QCOMPARE(value.toMap().value(QStringLiteral("awardProgram")).toString(),
                     QStringLiteral("WAZ"));
        }

        service.setAwardGoal(QStringLiteral("Worked"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QCOMPARE(service.roster().first().toMap()
                     .value(QStringLiteral("call")).toString(),
                 QStringLiteral("LIVEIL"));
        service.setRosterStatusFilter(QStringLiteral("All"));

        service.setRosterDxccIgnored(QStringLiteral("Israel"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 1, 5000);
        QCOMPARE(service.rosterPreferences().first().toMap()
                     .value(QStringLiteral("type")).toString(),
                 QStringLiteral("DXCC"));

        service.setRosterCallWatched(QStringLiteral("LIVEIT"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 2, 5000);
        service.removeRosterPreference(QStringLiteral("WATCH"),
                                       QStringLiteral("LIVEIT"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 1, 5000);

        service.setRosterCallIgnored(QStringLiteral("LIVEIT"), true);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterPreferenceCount(), 2, 5000);
        service.removeRosterPreference(QStringLiteral("CALL"),
                                       QStringLiteral("LIVEIT"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterCount(), 1, 5000);

        MapIntelligenceService persisted(nullptr, databasePath);
        persisted.setRosterStatusFilter(QStringLiteral("All"));
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 1, 5000);
        persisted.removeRosterPreference(QStringLiteral("DXCC"),
                                         QStringLiteral("Israel"));
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(persisted.rosterPreferenceCount(), 0, 5000);
    }

    void ingestsPskMqttIntoAnalyticsAndOperationalRoster()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           tempDir.path());

        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("psk-intelligence.sqlite")));
        auto* feed = qobject_cast<MapPskFeedService*>(service.pskFeedService());
        QVERIFY(feed);
        feed->configureStation(QStringLiteral("HOME"), QStringLiteral("JM75"));

        QVariantMap localDecode;
        localDecode.insert(QStringLiteral("timestamp"),
                           QDateTime::currentMSecsSinceEpoch());
        localDecode.insert(QStringLiteral("time"), QStringLiteral("120000"));
        localDecode.insert(QStringLiteral("message"), QStringLiteral("CQ MQTT1 JN70"));
        localDecode.insert(QStringLiteral("fromCall"), QStringLiteral("MQTT1"));
        localDecode.insert(QStringLiteral("dxGrid"), QStringLiteral("JN70"));
        localDecode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        localDecode.insert(QStringLiteral("db"), -9);
        localDecode.insert(QStringLiteral("freq"), 1500);
        localDecode.insert(QStringLiteral("isCQ"), true);
        service.ingestDecodeEntry(localDecode, 14074000, QStringLiteral("20m"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);

        QByteArray const payload = QByteArrayLiteral(
            R"json([{"rc":"MQTT1","rl":"JN70","rp":-7,"f":14074000,
                    "b":"20m","md":"FT8","t":)json")
            + QByteArray::number(QDateTime::currentSecsSinceEpoch())
            + QByteArrayLiteral("}]");
        QVERIFY(feed->injectPayloadForTest(payload));
        QTRY_VERIFY_WITH_TIMEOUT(feed->receivedCount() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(service.liveSpotCount() >= 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotHeatmap().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotTimeline().isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.spotPaths().isEmpty(), 5000);

        service.setSpotCorrelationFilter(QStringLiteral("Correlated"));
        QTRY_VERIFY_WITH_TIMEOUT(service.liveSpotCount() >= 1, 5000);
        service.setSpotCorrelationFilter(QStringLiteral("All"));

        service.setRosterRule(QStringLiteral("CALL"), QStringLiteral("MQTT1"),
                              QStringLiteral("WATCH"), QStringLiteral("20m"),
                              QStringLiteral("FT8"));
        QTRY_COMPARE_WITH_TIMEOUT(service.rosterRules().size(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.roster().isEmpty()
                                 && service.roster().first().toMap()
                                        .value(QStringLiteral("watched")).toBool(),
                                 5000);

        QStringList const visibleColumns {
            QStringLiteral("Grid"), QStringLiteral("POTA"),
            QStringLiteral("LoTW age")
        };
        service.setRosterVisibleColumns(visibleColumns);
        QCOMPARE(service.rosterVisibleColumns(), visibleColumns);
        QVERIFY(service.availableAwardPrograms().size() >= 328);
        QVERIFY(std::any_of(service.availableAwardPrograms().cbegin(),
                            service.availableAwardPrograms().cend(),
                            [](const QString& label) {
                                return label.contains(QStringLiteral("FT8DMC:"),
                                                      Qt::CaseInsensitive);
                            }));
    }

    void convertsWebMercatorAndRendersTropo()
    {
        QImage mercator(16, 16, QImage::Format_ARGB32);
        mercator.fill(QColor(20, 80, 160, 220));
        QImage const converted =
            MapExternalOverlayService::webMercatorToEquirectangular(
                mercator, QSize(64, 32));
        QCOMPARE(converted.size(), QSize(64, 32));
        QVERIFY(qAlpha(converted.pixel(32, 16)) > 0);
        QCOMPARE(qAlpha(converted.pixel(32, 0)), 0);

        QByteArray const tropo = R"json({
            "update_calls": [{
                "call": "TEST-1",
                "lat": 0.785398,
                "lon": 0.174533,
                "ts": 4102444800,
                "spokes": [
                    0.0, 0.04,
                    1.570796, 0.06,
                    3.141593, 0.05,
                    4.712389, 0.04
                ]
            }]
        })json";
        int featureCount = 0;
        QString error;
        QImage const tropoImage =
            MapExternalOverlayService::renderTropoPayload(
                tropo, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!tropoImage.isNull());

        bool hasVisiblePixel = false;
        for (int y = 0; y < tropoImage.height() && !hasVisiblePixel; ++y) {
            QRgb const* line =
                reinterpret_cast<QRgb const*>(tropoImage.constScanLine(y));
            for (int x = 0; x < tropoImage.width(); ++x) {
                if (qAlpha(line[x]) > 0) {
                    hasVisiblePixel = true;
                    break;
                }
            }
        }
        QVERIFY(hasVisiblePixel);

        QImage const moonImage =
            MapExternalOverlayService::renderMoonOverlay(
                35.9, 14.5, 128.0, 34.0, 384400.0, 63.0,
                QSize(256, 128));
        QCOMPARE(moonImage.size(), QSize(256, 128));
        bool hasMoonPixel = false;
        for (int y = 0; y < moonImage.height() && !hasMoonPixel; ++y) {
            QRgb const* line =
                reinterpret_cast<QRgb const*>(moonImage.constScanLine(y));
            for (int x = 0; x < moonImage.width(); ++x) {
                if (qAlpha(line[x]) > 0) {
                    hasMoonPixel = true;
                    break;
                }
            }
        }
        QVERIFY(hasMoonPixel);

        QTemporaryDir moonCache;
        QVERIFY(moonCache.isValid());
        MapLayerModel moonLayers;
        MapExternalOverlayService moonService(
            &moonLayers, nullptr, moonCache.path());
        moonLayers.setLayerEnabled(QStringLiteral("moon"), true);
        moonService.updateMoonForStation(35.9, 14.5);
        QVERIFY(moonService.moonDataAvailable());
        int moonLayerCount = -1;
        for (int row = 0; row < moonLayers.rowCount(); ++row) {
            QModelIndex const index = moonLayers.index(row, 0);
            if (moonLayers.data(index, MapLayerModel::LayerIdRole).toString()
                == QStringLiteral("moon")) {
                moonLayerCount = moonLayers.data(index, MapLayerModel::CountRole).toInt();
                break;
            }
        }
        QCOMPARE(moonLayerCount, 1);
        QVERIFY(moonService.moonAzimuth() >= 0.0);
        QVERIFY(moonService.moonAzimuth() < 360.0);
        QVERIFY(moonService.moonElevation() >= -90.0);
        QVERIFY(moonService.moonElevation() <= 90.0);
        QVERIFY(moonService.moonDistanceKm() > 300000.0);
        QVERIFY(moonService.moonDistanceKm() < 450000.0);
        QVERIFY(moonService.moonIllumination() >= 0.0);
        QVERIFY(moonService.moonIllumination() <= 100.0);
        QVERIFY(moonService.moonSublunarLatitude() >= -90.0);
        QVERIFY(moonService.moonSublunarLatitude() <= 90.0);
        QVERIFY(moonService.moonSublunarLongitude() >= -180.0);
        QVERIFY(moonService.moonSublunarLongitude() <= 180.0);
        QTRY_VERIFY_WITH_TIMEOUT(moonService.hasOverlay(), 5000);

        // The startup path restores layer preferences before the overlay
        // service is constructed.  A persisted Moon layer must refresh too.
        MapLayerModel persistedMoonLayers;
        persistedMoonLayers.setLayerEnabled(QStringLiteral("moon"), true);
        MapExternalOverlayService persistedMoonService(
            &persistedMoonLayers, nullptr, moonCache.path());
        persistedMoonService.updateMoonForStation(35.9, 14.5);
        QVERIFY(persistedMoonService.moonDataAvailable());

        QByteArray const earthquakes = R"json({
            "type": "FeatureCollection",
            "features": [{
                "type": "Feature",
                "properties": {"mag": 5.4, "title": "Test quake"},
                "geometry": {"type": "Point", "coordinates": [14.5, 35.9, 10]}
            }]
        })json";
        featureCount = 0;
        error.clear();
        QImage const earthquakeImage =
            MapExternalOverlayService::renderEarthquakePayload(
                earthquakes, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!earthquakeImage.isNull());

        QByteArray const wildfires = R"json({
            "events": [{
                "id": "WF-TEST",
                "title": "Test wildfire",
                "geometry": [
                    {"date": "2026-07-28T00:00:00Z",
                     "type": "Point", "coordinates": [-120.5, 38.2]}
                ]
            }]
        })json";
        featureCount = 0;
        error.clear();
        QImage const wildfireImage =
            MapExternalOverlayService::renderWildfirePayload(
                wildfires, &featureCount, &error, QSize(128, 64));
        QCOMPARE(featureCount, 1);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!wildfireImage.isNull());
    }

    void persistsLiveMapLayersAcrossRestarts()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        {
            QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            settings.clear();
            settings.beginGroup(QStringLiteral("LiveMapLayers"));
            // Simulate the old data-view side effect that saved Live as off.
            settings.setValue(QStringLiteral("Live"), false);
            settings.endGroup();
            settings.sync();
        }

        QString const databasePath =
            tempDir.filePath(QStringLiteral("map-intelligence.sqlite"));
        {
            MapIntelligenceService service(nullptr, databasePath);
            auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
            QVERIFY(layerModel);
            QVERIFY(layerModel->layerEnabled(QStringLiteral("live")));

            layerModel->setLayerEnabled(QStringLiteral("live"), false);
            layerModel->setLayerEnabled(QStringLiteral("moon"), true);
        }

        {
            MapIntelligenceService restored(nullptr, databasePath);
            auto* layerModel = qobject_cast<MapLayerModel*>(restored.layerModel());
            QVERIFY(layerModel);
            QVERIFY(!layerModel->layerEnabled(QStringLiteral("live")));
            QVERIFY(layerModel->layerEnabled(QStringLiteral("moon")));
        }
    }

    void replacesPskHeardBySnapshotWithoutClearingMqttFeed()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        MapIntelligenceService service(
            nullptr, tempDir.filePath(QStringLiteral("map-intelligence.sqlite")));
        auto* layerModel = qobject_cast<MapLayerModel*>(service.layerModel());
        QVERIFY(layerModel);

        auto layerCount = [layerModel](const QString& layerId) {
            for (int row = 0; row < layerModel->rowCount(); ++row) {
                QModelIndex const index = layerModel->index(row, 0);
                if (layerModel->data(index, MapLayerModel::LayerIdRole).toString() == layerId) {
                    return layerModel->data(index, MapLayerModel::CountRole).toInt();
                }
            }
            return -1;
        };

        QVariantMap mqtt;
        mqtt.insert(QStringLiteral("call"), QStringLiteral("MQTT-RX"));
        mqtt.insert(QStringLiteral("grid"), QStringLiteral("JO21"));
        mqtt.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        mqtt.insert(QStringLiteral("freq"), 14074000);
        mqtt.insert(QStringLiteral("provider"), QStringLiteral("PSK Reporter MQTT"));
        service.ingestPskSpots({mqtt}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);

        QVariantMap heardBy;
        heardBy.insert(QStringLiteral("call"), QStringLiteral("HTTP-RX"));
        heardBy.insert(QStringLiteral("grid"), QStringLiteral("JN58"));
        heardBy.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        heardBy.insert(QStringLiteral("freq"), 14074000);
        service.replacePskHeardBySpots(
            {heardBy}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("psk")), 2, 5000);

        // An empty HTTP reply is authoritative: it clears cached heard-by
        // listeners, but not the independent continuous MQTT feed.
        service.replacePskHeardBySpots({}, QStringLiteral("HOME"), QStringLiteral("JM75"));
        QTRY_COMPARE_WITH_TIMEOUT(service.liveSpotCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(layerCount(QStringLiteral("psk")), 1, 5000);
        QCOMPARE(service.coverageCells().size(), 1);
        QCOMPARE(service.coverageCells().first().toMap()
                     .value(QStringLiteral("grid")).toString(),
                 QStringLiteral("JO21"));
    }

    void aggregatesOperationalBandActivity()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());

        QString const databasePath =
            tempDir.filePath(QStringLiteral("band-activity.sqlite"));
        MapIntelligenceService service(nullptr, databasePath);
        service.setBandActivityWindowHours(1);
        QCOMPARE(service.bandActivityWindowHours(), 1);

        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        auto ingestLocal = [&service, now](QString const& call,
                                           QString const& grid,
                                           QString const& band,
                                           qint64 dialFrequencyHz,
                                           int offsetHz,
                                           int snr,
                                           bool isTx,
                                           int ageSeconds) {
            QVariantMap decode;
            decode.insert(QStringLiteral("timestamp"),
                          now - static_cast<qint64>(ageSeconds) * 1000);
            decode.insert(QStringLiteral("time"), QStringLiteral("120000"));
            decode.insert(QStringLiteral("message"),
                          QStringLiteral("CQ %1 %2").arg(call, grid));
            decode.insert(QStringLiteral("fromCall"), call);
            decode.insert(QStringLiteral("dxGrid"), grid);
            decode.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            decode.insert(QStringLiteral("db"), snr);
            decode.insert(QStringLiteral("freq"), offsetHz);
            decode.insert(QStringLiteral("isCQ"), true);
            decode.insert(QStringLiteral("isTx"), isTx);
            service.ingestDecodeEntry(decode, dialFrequencyHz, band);
        };

        ingestLocal(QStringLiteral("RX20A"), QStringLiteral("JN70"),
                    QStringLiteral("20m"), 14074000, 1450, -7, false, 35);
        ingestLocal(QStringLiteral("RX20B"), QStringLiteral("JO21"),
                    QStringLiteral("20m"), 14074000, 1550, -12, false, 20);
        ingestLocal(QStringLiteral("HOME"), QStringLiteral("JM75"),
                    QStringLiteral("20m"), 14074000, 1500, 0, true, 10);
        ingestLocal(QStringLiteral("RX40A"), QStringLiteral("IO91"),
                    QStringLiteral("40m"), 7074000, 1500, -18, false, 50);

        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("localRx")).toInt(),
            3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("localTx")).toInt(),
            1, 5000);

        QVariantList pskRows;
        auto pskRow = [now](QString const& call,
                            QString const& grid,
                            QString const& band,
                            qint64 frequencyHz,
                            QString const& direction,
                            int ageSeconds) {
            QVariantMap row;
            row.insert(QStringLiteral("call"), call);
            row.insert(QStringLiteral("grid"), grid);
            row.insert(QStringLiteral("band"), band);
            row.insert(QStringLiteral("freq"), frequencyHz);
            row.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
            row.insert(QStringLiteral("snr"), -10);
            row.insert(QStringLiteral("direction"), direction);
            row.insert(QStringLiteral("source"), QStringLiteral("psk"));
            row.insert(QStringLiteral("provider"), QStringLiteral("PSK Reporter"));
            row.insert(QStringLiteral("timestamp"),
                       now - static_cast<qint64>(ageSeconds) * 1000);
            return row;
        };
        pskRows.append(pskRow(QStringLiteral("PSK20A"), QStringLiteral("JN58"),
                              QStringLiteral("20m"), 14074000,
                              QStringLiteral("TX"), 15));
        pskRows.append(pskRow(QStringLiteral("PSK20B"), QStringLiteral("FN20"),
                              QStringLiteral("20m"), 14074000,
                              QStringLiteral("TX"), 5));
        pskRows.append(pskRow(QStringLiteral("PSK40A"), QStringLiteral("KO85"),
                              QStringLiteral("40m"), 7074000,
                              QStringLiteral("RX"), 25));
        service.ingestPskSpots(pskRows, QStringLiteral("HOME"),
                               QStringLiteral("JM75"));

        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("pskTx")).toInt(),
            2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            service.bandActivitySummary().value(QStringLiteral("pskRx")).toInt(),
            1, 5000);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("windowHours")).toInt(),
                 1);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("bandCount")).toInt(),
                 2);
        QCOMPARE(service.bandActivitySummary()
                     .value(QStringLiteral("bestBand")).toString(),
                 QStringLiteral("20m"));
        QVERIFY(service.bandActivitySummary()
                    .value(QStringLiteral("bestScore")).toInt() > 0);
        QCOMPARE(service.bandActivity().size(), 2);
        QVERIFY(!service.bandActivityTimeline().isEmpty());

        auto metricForBand = [&service](QString const& band) {
            for (QVariant const& value : service.bandActivity()) {
                QVariantMap const row = value.toMap();
                if (row.value(QStringLiteral("band")).toString() == band) {
                    return row;
                }
            }
            return QVariantMap {};
        };
        QVariantMap const twenty = metricForBand(QStringLiteral("20m"));
        QVERIFY(!twenty.isEmpty());
        QCOMPARE(twenty.value(QStringLiteral("rank")).toInt(), 1);
        QVERIFY(twenty.value(QStringLiteral("best")).toBool());
        QCOMPARE(twenty.value(QStringLiteral("localRx")).toInt(), 2);
        QCOMPARE(twenty.value(QStringLiteral("localTx")).toInt(), 1);
        QCOMPARE(twenty.value(QStringLiteral("pskRx")).toInt(), 0);
        QCOMPARE(twenty.value(QStringLiteral("pskTx")).toInt(), 2);
        QCOMPARE(twenty.value(QStringLiteral("uniqueCalls")).toInt(), 2);

        QVariantMap const forty = metricForBand(QStringLiteral("40m"));
        QVERIFY(!forty.isEmpty());
        QCOMPARE(forty.value(QStringLiteral("localRx")).toInt(), 1);
        QCOMPARE(forty.value(QStringLiteral("pskRx")).toInt(), 1);

        QVERIFY(databaseHasIndex(
            databasePath, QStringLiteral("idx_map_spot_event_band_window")));

        service.setBandActivityWindowHours(6);
        QCOMPARE(service.bandActivityWindowHours(), 6);
        service.setBandActivityWindowHours(12);
        QCOMPARE(service.bandActivityWindowHours(), 12);
        service.setBandActivityWindowHours(24);
        QCOMPARE(service.bandActivityWindowHours(), 24);
        service.setBandActivityWindowHours(5);
        QCOMPARE(service.bandActivityWindowHours(), 24);

        MapIntelligenceService persisted(nullptr, databasePath);
        QCOMPARE(persisted.bandActivityWindowHours(), 24);
    }
};

QTEST_MAIN(TestMapIntelligenceService)
#include "test_map_layer_service.moc"
