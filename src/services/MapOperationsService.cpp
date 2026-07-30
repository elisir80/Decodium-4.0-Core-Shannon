#include "MapOperationsService.h"

#include "MapLayerModel.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUdpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kNetworkTimeoutMs = 20000;
constexpr int kMaxPotaSpots = 1500;
constexpr int kMaxGeoFeatures = 5000;
constexpr int kMaxGeoPointsPerRing = 1200;
constexpr qint64 kIotaCacheMaxAgeSeconds = 30LL * 24LL * 60LL * 60LL;
constexpr auto kIotaCatalogUrl =
    "https://www.iota-world.org/islands-on-the-air/downloads/"
    "download-file.html?path=groups.json";

class ScopedMapDatabase
{
public:
    explicit ScopedMapDatabase(const QString& path)
        : m_name(QStringLiteral("map_operations_%1")
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(path);
    }

    ~ScopedMapDatabase()
    {
        if (m_database.isValid()) {
            m_database.close();
        }
        m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_name);
    }

    QSqlDatabase& database() { return m_database; }

private:
    QString m_name;
    QSqlDatabase m_database;
};

QString normalizedChoice(QString value, const QStringList& choices,
                         const QString& fallback)
{
    value = value.trimmed();
    for (QString const& choice : choices) {
        if (choice.compare(value, Qt::CaseInsensitive) == 0) {
            return choice;
        }
    }
    return fallback;
}

qint64 periodStartEpoch(const QString& period)
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    if (period.compare(QStringLiteral("24 hours"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-1).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("7 days"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-7).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("30 days"), Qt::CaseInsensitive) == 0) {
        return now.addDays(-30).toSecsSinceEpoch();
    }
    if (period.compare(QStringLiteral("1 year"), Qt::CaseInsensitive) == 0) {
        return now.addYears(-1).toSecsSinceEpoch();
    }
    return 0;
}

QString csvQuoted(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(value);
}

QPointF maidenheadCenter(QString grid)
{
    grid = grid.trimmed().toUpper();
    if (grid.size() < 4
        || grid.at(0) < QLatin1Char('A') || grid.at(0) > QLatin1Char('R')
        || grid.at(1) < QLatin1Char('A') || grid.at(1) > QLatin1Char('R')
        || !grid.at(2).isDigit() || !grid.at(3).isDigit()) {
        return {};
    }

    double lon = -180.0 + (grid.at(0).unicode() - QLatin1Char('A').unicode()) * 20.0
        + grid.at(2).digitValue() * 2.0;
    double lat = -90.0 + (grid.at(1).unicode() - QLatin1Char('A').unicode()) * 10.0
        + grid.at(3).digitValue();
    double lonSpan = 2.0;
    double latSpan = 1.0;
    if (grid.size() >= 6
        && grid.at(4) >= QLatin1Char('A') && grid.at(4) <= QLatin1Char('X')
        && grid.at(5) >= QLatin1Char('A') && grid.at(5) <= QLatin1Char('X')) {
        lonSpan = 2.0 / 24.0;
        latSpan = 1.0 / 24.0;
        lon += (grid.at(4).unicode() - QLatin1Char('A').unicode()) * lonSpan;
        lat += (grid.at(5).unicode() - QLatin1Char('A').unicode()) * latSpan;
    }
    return QPointF(lon + lonSpan / 2.0, lat + latSpan / 2.0);
}

double initialBearing(double latitude, double longitude,
                      double targetLatitude, double targetLongitude)
{
    double const lat1 = qDegreesToRadians(latitude);
    double const lat2 = qDegreesToRadians(targetLatitude);
    double const deltaLon = qDegreesToRadians(targetLongitude - longitude);
    double const y = qSin(deltaLon) * qCos(lat2);
    double const x = qCos(lat1) * qSin(lat2)
        - qSin(lat1) * qCos(lat2) * qCos(deltaLon);
    double bearing = qRadiansToDegrees(qAtan2(y, x));
    if (bearing < 0.0) {
        bearing += 360.0;
    }
    return bearing;
}

QString sortableColumn(const QString& value)
{
    QString const normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("call")) return QStringLiteral("call");
    if (normalized == QStringLiteral("band")) return QStringLiteral("band");
    if (normalized == QStringLiteral("mode")) return QStringLiteral("mode");
    if (normalized == QStringLiteral("dxcc")) return QStringLiteral("dxcc");
    if (normalized == QStringLiteral("grid")) return QStringLiteral("grid");
    if (normalized == QStringLiteral("frequency")) return QStringLiteral("frequency_mhz");
    if (normalized == QStringLiteral("status")) return QStringLiteral("confirmed");
    return QStringLiteral("qso_epoch");
}

struct SqlFilter {
    QString where;
    QVariantList binds;
};

SqlFilter buildFilter(const QString& search, const QString& band,
                      const QString& mode, const QString& period)
{
    QStringList clauses {QStringLiteral("1=1")};
    QVariantList binds;
    if (!band.trimmed().isEmpty()
        && band.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        clauses << QStringLiteral("lower(band)=lower(?)");
        binds << band.trimmed();
    }
    if (!mode.trimmed().isEmpty()
        && mode.compare(QStringLiteral("All"), Qt::CaseInsensitive) != 0) {
        clauses << QStringLiteral("upper(mode)=upper(?)");
        binds << mode.trimmed();
    }
    qint64 const start = periodStartEpoch(period);
    if (start > 0) {
        clauses << QStringLiteral("qso_epoch>=?");
        binds << start;
    }
    QString const text = search.trimmed();
    if (!text.isEmpty()) {
        clauses << QStringLiteral(
            "(call LIKE ? OR grid LIKE ? OR dxcc LIKE ? OR state LIKE ?"
            " OR pota_ref LIKE ? OR iota LIKE ? OR wpx LIKE ?)");
        QString const pattern = QStringLiteral("%%1%").arg(text);
        for (int i = 0; i < 7; ++i) {
            binds << pattern;
        }
    }
    return {clauses.join(QStringLiteral(" AND ")), binds};
}

bool prepareAndBind(QSqlQuery* query, const QString& sql,
                    const QVariantList& binds, QString* error)
{
    if (!query->prepare(sql)) {
        if (error) *error = query->lastError().text();
        return false;
    }
    for (QVariant const& bind : binds) {
        query->addBindValue(bind);
    }
    if (!query->exec()) {
        if (error) *error = query->lastError().text();
        return false;
    }
    return true;
}

QVariantMap rowToMap(QSqlQuery& query)
{
    QVariantMap row;
    row.insert(QStringLiteral("sourceKey"), query.value(0).toString());
    row.insert(QStringLiteral("call"), query.value(1).toString());
    row.insert(QStringLiteral("grid"), query.value(2).toString());
    row.insert(QStringLiteral("band"), query.value(3).toString());
    row.insert(QStringLiteral("mode"), query.value(4).toString());
    row.insert(QStringLiteral("date"), query.value(5).toString());
    row.insert(QStringLiteral("time"), query.value(6).toString());
    row.insert(QStringLiteral("epoch"), query.value(7).toLongLong());
    row.insert(QStringLiteral("frequencyMhz"), query.value(8).toDouble());
    row.insert(QStringLiteral("confirmed"), query.value(9).toBool());
    row.insert(QStringLiteral("dxcc"), query.value(10).toString());
    row.insert(QStringLiteral("continent"), query.value(11).toString());
    row.insert(QStringLiteral("state"), query.value(12).toString());
    row.insert(QStringLiteral("pota"), query.value(13).toString());
    row.insert(QStringLiteral("iota"), query.value(14).toString());
    row.insert(QStringLiteral("wpx"), query.value(15).toString());
    row.insert(QStringLiteral("source"), query.value(16).toString());
    return row;
}

QVariantList parseRings(const QJsonValue& coordinates)
{
    QVariantList rings;
    QJsonArray const outer = coordinates.toArray();
    for (QJsonValue const& ringValue : outer) {
        QJsonArray const rawRing = ringValue.toArray();
        if (rawRing.size() < 2) {
            continue;
        }
        int const stride = qMax(1, rawRing.size() / kMaxGeoPointsPerRing);
        QVariantList ring;
        ring.reserve(rawRing.size() / stride + 1);
        for (int index = 0; index < rawRing.size(); index += stride) {
            QJsonArray const point = rawRing.at(index).toArray();
            if (point.size() < 2) {
                continue;
            }
            // QVariantList << QVariantList concatenates both lists.  Keep the
            // GeoJSON coordinate as one nested QVariant so renderers receive
            // [longitude, latitude] points instead of a flattened number list.
            ring.append(QVariant::fromValue(QVariantList {
                point.at(0).toDouble(), point.at(1).toDouble()}));
        }
        if (ring.size() >= 2) {
            rings << QVariant::fromValue(ring);
        }
    }
    return rings;
}

QVariantMap presetMap(const QString& projection, const QString& dataView,
                      const QStringList& enabledLayers)
{
    return {
        {QStringLiteral("projection"), projection},
        {QStringLiteral("dataView"), dataView},
        {QStringLiteral("layers"), enabledLayers}
    };
}

} // namespace

MapOperationsService::MapOperationsService(const QString& databasePath,
                                           MapLayerModel* layerModel,
                                           QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_layerModel(layerModel)
    , m_network(new QNetworkAccessManager(this))
    , m_rotatorSocket(new QUdpSocket(this))
{
    loadSettings();
    loadMapPresets();

    if (m_layerModel) {
        connect(m_layerModel, &MapLayerModel::layerToggled, this,
                [this](QString const& id, bool enabled) {
            if (id == QStringLiteral("pota")) {
                if (enabled) {
                    refreshPota();
                } else {
                    clearSelectedPotaPark();
                    rebuildOperationalMarkers();
                }
            } else if (id == QStringLiteral("states")
                       || id == QStringLiteral("counties")) {
                // Rebuild for both transitions.  The feature cache remains in
                // memory, but disabled boundaries must leave the renderer now.
                refreshGeographicFeatures();
            } else if (id == QStringLiteral("iota")
                       || id == QStringLiteral("wpx")) {
                if (enabled && id == QStringLiteral("iota")) {
                    ensureIotaCatalog();
                }
                rebuildOperationalMarkers();
            }
        });
    }

    refreshLogbook();
    if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("pota"))) {
        refreshPota();
    }
    if (m_layerModel
        && (m_layerModel->layerEnabled(QStringLiteral("states"))
            || m_layerModel->layerEnabled(QStringLiteral("counties")))) {
        refreshGeographicFeatures();
    }
    if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("iota"))) {
        ensureIotaCatalog();
    }
}

MapOperationsService::~MapOperationsService()
{
    ++m_logbookGeneration;
    ++m_geoGeneration;
    ++m_iotaGeneration;
}

QStringList MapOperationsService::availableProjections() const
{
    return {
        QStringLiteral("Equirectangular"),
        QStringLiteral("Mercator"),
        QStringLiteral("Miller"),
        QStringLiteral("Azimuthal Equidistant")
    };
}

QStringList MapOperationsService::availableDataViews() const
{
    return {
        QStringLiteral("Live"),
        QStringLiteral("Logbook"),
        QStringLiteral("Live + Logbook")
    };
}

void MapOperationsService::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapOperations"));
    m_mapProjection = normalizedChoice(
        settings.value(QStringLiteral("Projection"),
                       QStringLiteral("Equirectangular")).toString(),
        availableProjections(), QStringLiteral("Equirectangular"));
    m_dataViewMode = normalizedChoice(
        settings.value(QStringLiteral("DataView"),
                       QStringLiteral("Live + Logbook")).toString(),
        availableDataViews(), QStringLiteral("Live + Logbook"));
    m_activeMapPreset =
        settings.value(QStringLiteral("ActivePreset"),
                       QStringLiteral("Operational")).toString();
    m_logbookBand =
        settings.value(QStringLiteral("LogbookBand"), QStringLiteral("All")).toString();
    m_logbookMode =
        settings.value(QStringLiteral("LogbookMode"), QStringLiteral("All")).toString();
    m_logbookPeriod =
        settings.value(QStringLiteral("LogbookPeriod"),
                       QStringLiteral("All time")).toString();
    m_logbookSort =
        settings.value(QStringLiteral("LogbookSort"), QStringLiteral("Date")).toString();
    m_logbookSortDescending =
        settings.value(QStringLiteral("LogbookSortDescending"), true).toBool();
    m_logbookLimit =
        qBound(50, settings.value(QStringLiteral("LogbookLimit"), 500).toInt(), 5000);
    m_rotatorHost =
        settings.value(QStringLiteral("RotatorHost"),
                       QStringLiteral("127.0.0.1")).toString().trimmed();
    m_rotatorPort =
        qBound(1, settings.value(QStringLiteral("RotatorPort"), 12040).toInt(), 65535);
    m_rotatorEnabled =
        settings.value(QStringLiteral("RotatorEnabled"), false).toBool();
    settings.endGroup();
    m_rotatorStatus = m_rotatorEnabled
        ? QStringLiteral("PSTRotator ready")
        : QStringLiteral("Rotator disabled");
}

void MapOperationsService::saveSetting(const QString& key,
                                       const QVariant& value) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapOperations"));
    settings.setValue(key, value);
    settings.endGroup();
}

void MapOperationsService::loadMapPresets()
{
    m_mapPresets = {
        QStringLiteral("Operational"), QStringLiteral("Logbook"),
        QStringLiteral("Parks"), QStringLiteral("Awards"),
        QStringLiteral("Propagation"), QStringLiteral("Minimal")
    };
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    for (QString const& name : settings.childGroups()) {
        if (!m_mapPresets.contains(name, Qt::CaseInsensitive)) {
            m_mapPresets << name;
        }
    }
    settings.endGroup();
    m_mapPresets.sort(Qt::CaseInsensitive);
    emit mapPresetsChanged();
}

void MapOperationsService::setMapProjection(const QString& projection)
{
    QString const normalized = normalizedChoice(
        projection, availableProjections(), QStringLiteral("Equirectangular"));
    if (m_mapProjection == normalized) {
        return;
    }
    m_mapProjection = normalized;
    saveSetting(QStringLiteral("Projection"), normalized);
    m_activeMapPreset.clear();
    emit mapProjectionChanged();
    emit activeMapPresetChanged();
}

void MapOperationsService::setDataViewMode(const QString& mode)
{
    QString const normalized = normalizedChoice(
        mode, availableDataViews(), QStringLiteral("Live + Logbook"));
    if (m_dataViewMode == normalized) {
        return;
    }
    m_dataViewMode = normalized;
    // Data view controls the operations panel only. Layer choices belong to
    // the map and must remain independent and persistent across restarts.
    saveSetting(QStringLiteral("DataView"), normalized);
    emit dataViewModeChanged();
}

void MapOperationsService::setLogbookSearch(const QString& value)
{
    QString const normalized = value.trimmed().left(80);
    if (m_logbookSearch == normalized) return;
    m_logbookSearch = normalized;
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookBand(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed();
    if (m_logbookBand == normalized) return;
    m_logbookBand = normalized;
    saveSetting(QStringLiteral("LogbookBand"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookMode(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed();
    if (m_logbookMode == normalized) return;
    m_logbookMode = normalized;
    saveSetting(QStringLiteral("LogbookMode"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookPeriod(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All time") : value.trimmed();
    if (m_logbookPeriod == normalized) return;
    m_logbookPeriod = normalized;
    saveSetting(QStringLiteral("LogbookPeriod"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookSort(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("Date") : value.trimmed();
    if (m_logbookSort == normalized) return;
    m_logbookSort = normalized;
    saveSetting(QStringLiteral("LogbookSort"), normalized);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookSortDescending(bool descending)
{
    if (m_logbookSortDescending == descending) return;
    m_logbookSortDescending = descending;
    saveSetting(QStringLiteral("LogbookSortDescending"), descending);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setLogbookLimit(int limit)
{
    int const bounded = qBound(50, limit, 5000);
    if (m_logbookLimit == bounded) return;
    m_logbookLimit = bounded;
    saveSetting(QStringLiteral("LogbookLimit"), bounded);
    emit logbookFiltersChanged();
    refreshLogbook();
}

void MapOperationsService::setRotatorHost(const QString& host)
{
    QString const normalized = host.trimmed().left(255);
    if (normalized.isEmpty() || m_rotatorHost == normalized) return;
    m_rotatorHost = normalized;
    saveSetting(QStringLiteral("RotatorHost"), normalized);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorPort(int port)
{
    int const bounded = qBound(1, port, 65535);
    if (m_rotatorPort == bounded) return;
    m_rotatorPort = bounded;
    saveSetting(QStringLiteral("RotatorPort"), bounded);
    emit rotatorSettingsChanged();
}

void MapOperationsService::setRotatorEnabled(bool enabled)
{
    if (m_rotatorEnabled == enabled) return;
    m_rotatorEnabled = enabled;
    saveSetting(QStringLiteral("RotatorEnabled"), enabled);
    setRotatorStatus(enabled ? QStringLiteral("PSTRotator ready")
                             : QStringLiteral("Rotator disabled"));
    emit rotatorSettingsChanged();
}

void MapOperationsService::refreshPota()
{
    if (m_potaLoading) {
        return;
    }
    setPotaLoading(true);
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.pota.app/spot/activator")));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handlePotaReply(reply); });
}

void MapOperationsService::handlePotaReply(QNetworkReply* reply)
{
    QByteArray const bytes = reply->readAll();
    QString const networkError = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    setPotaLoading(false);
    if (!networkError.isEmpty()) {
        setStatusMessage(QStringLiteral("POTA update failed: %1").arg(networkError));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        setStatusMessage(QStringLiteral("POTA returned invalid data"));
        return;
    }

    QVariantList spots;
    QVariantList markers;
    QJsonArray const values = document.array();
    spots.reserve(qMin(values.size(), kMaxPotaSpots));
    markers.reserve(qMin(values.size(), kMaxPotaSpots));
    for (QJsonValue const& value : values) {
        if (spots.size() >= kMaxPotaSpots || !value.isObject()) break;
        QVariantMap spot = value.toObject().toVariantMap();
        QString const reference = spot.value(QStringLiteral("reference")).toString().toUpper();
        bool latitudeOk = false;
        bool longitudeOk = false;
        double const latitude =
            spot.value(QStringLiteral("latitude")).toDouble(&latitudeOk);
        double const longitude =
            spot.value(QStringLiteral("longitude")).toDouble(&longitudeOk);
        if (reference.isEmpty() || !latitudeOk || !longitudeOk
            || qAbs(latitude) > 90.0 || qAbs(longitude) > 180.0) {
            continue;
        }
        spot.insert(QStringLiteral("reference"), reference);
        spot.insert(QStringLiteral("parkName"),
                    spot.value(QStringLiteral("name")).toString());
        spots << spot;
        markers << markerFromPotaSpot(spot);
    }
    m_potaSpots = spots;
    m_potaMarkers = markers;
    if (m_layerModel) {
        m_layerModel->setCount(QStringLiteral("pota"), spots.size());
    }
    rebuildOperationalMarkers();
    emit potaSpotsChanged();
    setStatusMessage(QStringLiteral("POTA: %1 active parks").arg(spots.size()));
}

void MapOperationsService::selectPotaPark(const QString& reference)
{
    QString const normalized = reference.trimmed().toUpper();
    if (normalized.isEmpty()) return;

    for (QVariant const& value : std::as_const(m_potaSpots)) {
        QVariantMap spot = value.toMap();
        if (spot.value(QStringLiteral("reference")).toString() == normalized) {
            m_selectedPotaPark = spot;
            emit selectedPotaParkChanged();
            break;
        }
    }

    QNetworkRequest request(
        QUrl(QStringLiteral("https://api.pota.app/park/%1").arg(normalized)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handlePotaParkReply(reply); });
}

void MapOperationsService::handlePotaParkReply(QNetworkReply* reply)
{
    QByteArray const bytes = reply->readAll();
    bool const ok = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!ok) return;
    QJsonDocument const document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) return;
    QVariantMap details = document.object().toVariantMap();
    for (auto it = m_selectedPotaPark.constBegin();
         it != m_selectedPotaPark.constEnd(); ++it) {
        if (!details.contains(it.key())) details.insert(it.key(), it.value());
    }
    m_selectedPotaPark = details;
    emit selectedPotaParkChanged();
}

void MapOperationsService::clearSelectedPotaPark()
{
    if (m_selectedPotaPark.isEmpty()) return;
    m_selectedPotaPark.clear();
    emit selectedPotaParkChanged();
}

QVariantMap MapOperationsService::markerFromPotaSpot(const QVariantMap& spot)
{
    QString const reference =
        spot.value(QStringLiteral("reference")).toString().toUpper();
    QString const activator =
        spot.value(QStringLiteral("activator")).toString().toUpper();
    return {
        {QStringLiteral("id"), QStringLiteral("pota:%1:%2").arg(reference, activator)},
        {QStringLiteral("type"), QStringLiteral("POTA")},
        {QStringLiteral("reference"), reference},
        {QStringLiteral("call"), activator},
        {QStringLiteral("label"), reference},
        {QStringLiteral("latitude"), spot.value(QStringLiteral("latitude"))},
        {QStringLiteral("longitude"), spot.value(QStringLiteral("longitude"))},
        {QStringLiteral("grid"), spot.value(QStringLiteral("grid6"))},
        {QStringLiteral("name"), spot.value(QStringLiteral("parkName"))},
        {QStringLiteral("frequency"), spot.value(QStringLiteral("frequency"))},
        {QStringLiteral("mode"), spot.value(QStringLiteral("mode"))},
        {QStringLiteral("comments"), spot.value(QStringLiteral("comments"))},
        {QStringLiteral("color"), QStringLiteral("#74d66a")}
    };
}

QString MapOperationsService::iotaCachePath() const
{
    QFileInfo const databaseInfo(m_databasePath);
    return databaseInfo.dir().filePath(QStringLiteral("iota_groups.json"));
}

void MapOperationsService::ensureIotaCatalog()
{
    if (m_iotaLoading || !m_iotaCatalogMarkers.isEmpty()) {
        return;
    }

    QString const cachePath = iotaCachePath();
    QFile cache(cachePath);
    if (cache.open(QIODevice::ReadOnly)) {
        QByteArray const data = cache.readAll();
        QFileInfo const cacheInfo(cachePath);
        qint64 const ageSeconds =
            cacheInfo.lastModified().toUTC().secsTo(QDateTime::currentDateTimeUtc());
        bool const stale = ageSeconds < 0
            || ageSeconds > kIotaCacheMaxAgeSeconds;
        parseIotaCatalog(data, false, stale);
        return;
    }

    requestIotaCatalog();
}

void MapOperationsService::refreshIotaCatalog()
{
    requestIotaCatalog();
}

void MapOperationsService::requestIotaCatalog()
{
    if (m_iotaLoading) {
        return;
    }
    m_iotaLoading = true;
    QNetworkRequest request(QUrl(QString::fromLatin1(kIotaCatalogUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply] { handleIotaCatalogReply(reply); });
}

void MapOperationsService::handleIotaCatalogReply(QNetworkReply* reply)
{
    QByteArray const bytes = reply->readAll();
    QString const networkError = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    if (!networkError.isEmpty()) {
        m_iotaLoading = false;
        setStatusMessage(
            m_iotaCatalogMarkers.isEmpty()
                ? QStringLiteral("IOTA catalog update failed: %1")
                      .arg(networkError)
                : QStringLiteral("IOTA catalog update failed; cached groups remain available"));
        return;
    }
    parseIotaCatalog(bytes, true, false);
}

void MapOperationsService::parseIotaCatalog(const QByteArray& data,
                                            bool persist,
                                            bool refreshAfterParse)
{
    m_iotaLoading = true;
    quint64 const generation = ++m_iotaGeneration;
    auto* watcher = new QFutureWatcher<IotaSnapshot>(this);
    connect(watcher, &QFutureWatcher<IotaSnapshot>::finished, this,
            [this, watcher, generation, data, persist, refreshAfterParse] {
        IotaSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        if (generation != m_iotaGeneration.load()) {
            return;
        }
        m_iotaLoading = false;
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(snapshot.error);
            if (!persist && m_iotaCatalogMarkers.isEmpty()) {
                requestIotaCatalog();
            }
            return;
        }

        m_iotaCatalogMarkers = snapshot.markers;
        if (persist) {
            QString const cachePath = iotaCachePath();
            QDir().mkpath(QFileInfo(cachePath).absolutePath());
            QSaveFile cache(cachePath);
            if (cache.open(QIODevice::WriteOnly)) {
                cache.write(data);
                cache.commit();
            }
        }
        rebuildOperationalMarkers();
        setStatusMessage(QStringLiteral("IOTA: %1 catalog groups")
                             .arg(m_iotaCatalogMarkers.size()));
        if (refreshAfterParse) {
            requestIotaCatalog();
        }
    });
    watcher->setFuture(QtConcurrent::run(
        [data] { return parseIotaCatalogData(data); }));
}

MapOperationsService::IotaSnapshot
MapOperationsService::parseIotaCatalogData(const QByteArray& data)
{
    IotaSnapshot snapshot;
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        snapshot.error = QStringLiteral("IOTA catalog contains invalid JSON");
        return snapshot;
    }

    static QRegularExpression const referencePattern(
        QStringLiteral("^[A-Z]{2}-\\d{3}$"));
    QJsonArray const groups = document.array();
    snapshot.markers.reserve(groups.size());
    for (QJsonValue const& value : groups) {
        if (!value.isObject()) {
            continue;
        }
        QJsonObject const group = value.toObject();
        QString const reference =
            group.value(QStringLiteral("refno")).toString().trimmed().toUpper();
        if (!referencePattern.match(reference).hasMatch()) {
            continue;
        }

        bool latitudeMinOk = false;
        bool latitudeMaxOk = false;
        bool longitudeMinOk = false;
        bool longitudeMaxOk = false;
        double const latitudeMin =
            group.value(QStringLiteral("latitude_min")).toVariant()
                .toString().toDouble(&latitudeMinOk);
        double const latitudeMax =
            group.value(QStringLiteral("latitude_max")).toVariant()
                .toString().toDouble(&latitudeMaxOk);
        double const longitudeMin =
            group.value(QStringLiteral("longitude_min")).toVariant()
                .toString().toDouble(&longitudeMinOk);
        double const longitudeMax =
            group.value(QStringLiteral("longitude_max")).toVariant()
                .toString().toDouble(&longitudeMaxOk);
        if (!latitudeMinOk || !latitudeMaxOk || !longitudeMinOk
            || !longitudeMaxOk
            || qAbs(latitudeMin) > 90.0 || qAbs(latitudeMax) > 90.0
            || qAbs(longitudeMin) > 180.0 || qAbs(longitudeMax) > 180.0) {
            continue;
        }

        double longitudeA = longitudeMin;
        double longitudeB = longitudeMax;
        if (qAbs(longitudeA - longitudeB) > 180.0) {
            if (longitudeA < 0.0) longitudeA += 360.0;
            if (longitudeB < 0.0) longitudeB += 360.0;
        }
        double longitude = 0.5 * (longitudeA + longitudeB);
        if (longitude > 180.0) longitude -= 360.0;
        double const latitude = 0.5 * (latitudeMin + latitudeMax);
        QString const name =
            group.value(QStringLiteral("name")).toString().trimmed();
        QString const comment =
            group.value(QStringLiteral("comment")).toString().trimmed();

        snapshot.markers << QVariantMap {
            {QStringLiteral("id"),
             QStringLiteral("iota-catalog:%1").arg(reference)},
            {QStringLiteral("type"), QStringLiteral("IOTA")},
            {QStringLiteral("reference"), reference},
            {QStringLiteral("label"), reference},
            {QStringLiteral("name"), name},
            {QStringLiteral("latitude"), latitude},
            {QStringLiteral("longitude"), longitude},
            {QStringLiteral("latitudeMin"), latitudeMin},
            {QStringLiteral("latitudeMax"), latitudeMax},
            {QStringLiteral("longitudeMin"), longitudeMin},
            {QStringLiteral("longitudeMax"), longitudeMax},
            {QStringLiteral("dxcc"),
             group.value(QStringLiteral("dxcc_num")).toString()},
            {QStringLiteral("creditedPercent"),
             group.value(QStringLiteral("pc_credited")).toString()},
            {QStringLiteral("comments"),
             comment.isEmpty()
                 ? QStringLiteral("Official IOTA Directory catalog group")
                 : comment},
            {QStringLiteral("source"), QStringLiteral("IOTA Directory")},
            {QStringLiteral("catalog"), true},
            {QStringLiteral("worked"), false},
            {QStringLiteral("confirmed"), false},
            {QStringLiteral("color"), QStringLiteral("#44d7e8")}
        };
    }
    if (snapshot.markers.isEmpty()) {
        snapshot.error = QStringLiteral("IOTA catalog contains no usable groups");
    }
    return snapshot;
}

void MapOperationsService::refreshGeographicFeatures()
{
    if (!m_layerModel) return;
    // Invalidate a reply for a layer that was turned off while its network
    // request or GeoJSON parsing task was still in flight.
    for (QString const& layerId : {QStringLiteral("states"),
                                  QStringLiteral("counties")}) {
        if (!m_layerModel->layerEnabled(layerId)) {
            m_geoLayerGeneration.insert(layerId, ++m_geoGeneration);
        }
    }
    bool requested = false;
    if (m_layerModel->layerEnabled(QStringLiteral("states"))
        && m_stateFeatures.isEmpty()) {
        QUrl url(QStringLiteral(
            "https://tigerweb.geo.census.gov/arcgis/rest/services/"
            "TIGERweb/State_County/MapServer/15/query"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("where"), QStringLiteral("1=1"));
        query.addQueryItem(QStringLiteral("outFields"),
                           QStringLiteral("STATE,STUSAB,BASENAME,NAME"));
        query.addQueryItem(QStringLiteral("returnGeometry"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("maxAllowableOffset"), QStringLiteral("0.04"));
        query.addQueryItem(QStringLiteral("geometryPrecision"), QStringLiteral("3"));
        query.addQueryItem(QStringLiteral("outSR"), QStringLiteral("4326"));
        query.addQueryItem(QStringLiteral("f"), QStringLiteral("geojson"));
        url.setQuery(query);
        requestGeoLayer(QStringLiteral("states"), url);
        requested = true;
    }
    if (m_layerModel->layerEnabled(QStringLiteral("counties"))
        && m_countyFeatures.isEmpty()) {
        QUrl url(QStringLiteral(
            "https://tigerweb.geo.census.gov/arcgis/rest/services/"
            "TIGERweb/State_County/MapServer/13/query"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("where"), QStringLiteral("1=1"));
        query.addQueryItem(QStringLiteral("outFields"),
                           QStringLiteral("STATE,COUNTY,BASENAME,NAME"));
        query.addQueryItem(QStringLiteral("returnGeometry"), QStringLiteral("true"));
        query.addQueryItem(QStringLiteral("maxAllowableOffset"), QStringLiteral("0.03"));
        query.addQueryItem(QStringLiteral("geometryPrecision"), QStringLiteral("3"));
        query.addQueryItem(QStringLiteral("outSR"), QStringLiteral("4326"));
        query.addQueryItem(QStringLiteral("f"), QStringLiteral("geojson"));
        url.setQuery(query);
        requestGeoLayer(QStringLiteral("counties"), url);
        requested = true;
    }
    if (!requested) {
        m_geographicFeatures.clear();
        if (m_layerModel->layerEnabled(QStringLiteral("states"))) {
            m_geographicFeatures.append(m_stateFeatures);
        }
        if (m_layerModel->layerEnabled(QStringLiteral("counties"))) {
            m_geographicFeatures.append(m_countyFeatures);
        }
        emit geographicFeaturesChanged();
    }
}

void MapOperationsService::requestGeoLayer(const QString& layerId,
                                           const QUrl& url)
{
    if (m_geoPendingLayers.contains(layerId)) {
        return;
    }
    quint64 const generation = ++m_geoGeneration;
    m_geoLayerGeneration.insert(layerId, generation);
    m_geoPendingLayers.insert(layerId);
    setGeographicLoading(true);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Decodium4 Map Intelligence"));
    request.setTransferTimeout(kNetworkTimeoutMs);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, layerId, generation, reply] {
                handleGeoReply(layerId, generation, reply);
            });
}

void MapOperationsService::handleGeoReply(const QString& layerId,
                                          quint64 generation,
                                          QNetworkReply* reply)
{
    QByteArray const bytes = reply->readAll();
    QString const error = reply->error() == QNetworkReply::NoError
        ? QString() : reply->errorString();
    reply->deleteLater();
    if (!error.isEmpty()) {
        m_geoPendingLayers.remove(layerId);
        setGeographicLoading(!m_geoPendingLayers.isEmpty());
        setStatusMessage(QStringLiteral("%1 boundaries failed: %2")
                             .arg(layerId, error));
        return;
    }

    auto* watcher = new QFutureWatcher<GeoSnapshot>(this);
    connect(watcher, &QFutureWatcher<GeoSnapshot>::finished, this,
            [this, watcher, generation, layerId] {
        GeoSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        m_geoPendingLayers.remove(layerId);
        setGeographicLoading(!m_geoPendingLayers.isEmpty());
        bool const stale = m_geoLayerGeneration.value(layerId) != generation;
        if (stale) {
            if (m_layerModel && m_layerModel->layerEnabled(layerId)) {
                refreshGeographicFeatures();
            }
            return;
        }
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(snapshot.error);
            return;
        }
        if (layerId == QStringLiteral("states")) {
            m_stateFeatures = snapshot.features;
        } else {
            m_countyFeatures = snapshot.features;
        }
        if (m_layerModel) {
            m_layerModel->setCount(layerId, snapshot.features.size());
        }
        m_geographicFeatures.clear();
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("states"))) {
            m_geographicFeatures.append(m_stateFeatures);
        }
        if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("counties"))) {
            m_geographicFeatures.append(m_countyFeatures);
        }
        emit geographicFeaturesChanged();
        setStatusMessage(QStringLiteral("%1: %2 boundaries")
                             .arg(layerId).arg(snapshot.features.size()));
    });
    watcher->setFuture(QtConcurrent::run(
        [bytes, layerId] { return parseGeoJson(bytes, layerId); }));
}

MapOperationsService::GeoSnapshot
MapOperationsService::parseGeoJson(const QByteArray& data,
                                   const QString& layerId)
{
    GeoSnapshot snapshot;
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        snapshot.error = QStringLiteral("%1 boundaries contain invalid GeoJSON")
                             .arg(layerId);
        return snapshot;
    }
    QJsonArray const features =
        document.object().value(QStringLiteral("features")).toArray();
    for (QJsonValue const& value : features) {
        if (snapshot.features.size() >= kMaxGeoFeatures || !value.isObject()) break;
        QJsonObject const feature = value.toObject();
        QJsonObject const geometry =
            feature.value(QStringLiteral("geometry")).toObject();
        QJsonObject const properties =
            feature.value(QStringLiteral("properties")).toObject();
        QString const geometryType =
            geometry.value(QStringLiteral("type")).toString();
        QVariantList polygonRings;
        if (geometryType == QStringLiteral("Polygon")) {
            QVariantList const rings =
                parseRings(geometry.value(QStringLiteral("coordinates")));
            if (!rings.isEmpty()) polygonRings << QVariant::fromValue(rings);
        } else if (geometryType == QStringLiteral("MultiPolygon")) {
            for (QJsonValue const& polygon :
                 geometry.value(QStringLiteral("coordinates")).toArray()) {
                QVariantList const rings = parseRings(polygon);
                if (!rings.isEmpty()) polygonRings << QVariant::fromValue(rings);
            }
        }
        if (polygonRings.isEmpty()) continue;
        QString const state = properties.value(QStringLiteral("STUSAB")).toString();
        QString const county = properties.value(QStringLiteral("BASENAME")).toString();
        QString const label = layerId == QStringLiteral("states")
            ? (!state.isEmpty() ? state : properties.value(QStringLiteral("NAME")).toString())
            : QStringLiteral("%1, %2").arg(county, state);
        QVariantMap row {
            {QStringLiteral("id"),
             QStringLiteral("%1:%2:%3")
                 .arg(layerId,
                      properties.value(QStringLiteral("STATE")).toVariant().toString(),
                      properties.value(QStringLiteral("COUNTY")).toVariant().toString())},
            {QStringLiteral("type"), layerId},
            {QStringLiteral("label"), label},
            {QStringLiteral("state"), state},
            {QStringLiteral("county"), county},
            {QStringLiteral("polygons"), polygonRings},
            {QStringLiteral("color"),
             layerId == QStringLiteral("states")
                 ? QStringLiteral("#58b8d6") : QStringLiteral("#7c91a8")}
        };
        snapshot.features << row;
    }
    return snapshot;
}

void MapOperationsService::refreshLogbook()
{
    quint64 const generation = ++m_logbookGeneration;
    setLogbookLoading(true);
    QString const databasePath = m_databasePath;
    QString const search = m_logbookSearch;
    QString const band = m_logbookBand;
    QString const mode = m_logbookMode;
    QString const period = m_logbookPeriod;
    QString const sort = m_logbookSort;
    bool const descending = m_logbookSortDescending;
    int const limit = m_logbookLimit;

    auto* watcher = new QFutureWatcher<LogbookSnapshot>(this);
    connect(watcher, &QFutureWatcher<LogbookSnapshot>::finished, this,
            [this, watcher, generation] {
        LogbookSnapshot snapshot = watcher->result();
        watcher->deleteLater();
        if (generation != m_logbookGeneration.load()) return;
        setLogbookLoading(false);
        if (!snapshot.error.isEmpty()) {
            setStatusMessage(QStringLiteral("Logbook: %1").arg(snapshot.error));
            return;
        }
        m_logbookRows = snapshot.rows;
        m_logbookTotal = snapshot.total;
        m_scorecard = snapshot.scorecard;
        m_chartData = snapshot.chartData;
        m_comparison = snapshot.comparison;
        m_databaseMarkers = snapshot.markers;
        rebuildOperationalMarkers();
        emit logbookChanged();
        emit statisticsChanged();
    });
    watcher->setFuture(QtConcurrent::run(
        [databasePath, search, band, mode, period, sort, descending, limit] {
        return queryLogbookDatabase(databasePath, search, band, mode, period,
                                    sort, descending, limit);
    }));
}

MapOperationsService::LogbookSnapshot
MapOperationsService::queryLogbookDatabase(
    const QString& databasePath, const QString& search,
    const QString& band, const QString& mode, const QString& period,
    const QString& sort, bool descending, int limit)
{
    LogbookSnapshot snapshot;
    ScopedMapDatabase connection(databasePath);
    QSqlDatabase& db = connection.database();
    if (!db.open()) {
        snapshot.error = db.lastError().text();
        return snapshot;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    SqlFilter const filter = buildFilter(search, band, mode, period);
    QSqlQuery countQuery(db);
    if (!prepareAndBind(&countQuery,
                        QStringLiteral("SELECT count(*) FROM map_qso WHERE %1")
                            .arg(filter.where),
                        filter.binds, &snapshot.error)) {
        return snapshot;
    }
    if (countQuery.next()) snapshot.total = countQuery.value(0).toInt();

    QString const selectSql = QStringLiteral(
        "SELECT source_key,call,grid,band,mode,qso_date,time_on,qso_epoch,"
        " frequency_mhz,confirmed,dxcc,continent,state,pota_ref,iota,wpx,source"
        " FROM map_qso WHERE %1 ORDER BY %2 %3 LIMIT %4")
        .arg(filter.where, sortableColumn(sort),
             descending ? QStringLiteral("DESC") : QStringLiteral("ASC"))
        .arg(qBound(50, limit, 5000));
    QSqlQuery rowQuery(db);
    if (!prepareAndBind(&rowQuery, selectSql, filter.binds, &snapshot.error)) {
        return snapshot;
    }
    while (rowQuery.next()) {
        snapshot.rows << rowToMap(rowQuery);
    }

    QSqlQuery scoreQuery(db);
    QString const scoreSql = QStringLiteral(
        "SELECT count(*),sum(CASE WHEN confirmed<>0 THEN 1 ELSE 0 END),"
        " count(DISTINCT upper(call)),count(DISTINCT upper(dxcc)),"
        " count(DISTINCT upper(grid4)),"
        " count(DISTINCT CASE WHEN pota_ref<>'' THEN upper(pota_ref) END),"
        " count(DISTINCT CASE WHEN iota<>'' THEN upper(iota) END),"
        " count(DISTINCT CASE WHEN wpx<>'' THEN upper(wpx) END)"
        " FROM map_qso WHERE %1").arg(filter.where);
    if (prepareAndBind(&scoreQuery, scoreSql, filter.binds, nullptr)
        && scoreQuery.next()) {
        snapshot.scorecard = {
            {QStringLiteral("qsos"), scoreQuery.value(0).toInt()},
            {QStringLiteral("confirmed"), scoreQuery.value(1).toInt()},
            {QStringLiteral("calls"), scoreQuery.value(2).toInt()},
            {QStringLiteral("dxcc"), scoreQuery.value(3).toInt()},
            {QStringLiteral("grids"), scoreQuery.value(4).toInt()},
            {QStringLiteral("pota"), scoreQuery.value(5).toInt()},
            {QStringLiteral("iota"), scoreQuery.value(6).toInt()},
            {QStringLiteral("wpx"), scoreQuery.value(7).toInt()}
        };
    }

    auto appendChart = [&](QString const& group, QString const& column) {
        QSqlQuery chartQuery(db);
        QString const sql = QStringLiteral(
            "SELECT coalesce(nullif(%1,''),'Unknown'),count(*),"
            " sum(CASE WHEN confirmed<>0 THEN 1 ELSE 0 END)"
            " FROM map_qso WHERE %2 GROUP BY 1 ORDER BY count(*) DESC LIMIT 30")
            .arg(column, filter.where);
        if (!prepareAndBind(&chartQuery, sql, filter.binds, nullptr)) return;
        while (chartQuery.next()) {
            snapshot.chartData << QVariantMap {
                {QStringLiteral("group"), group},
                {QStringLiteral("label"), chartQuery.value(0).toString()},
                {QStringLiteral("worked"), chartQuery.value(1).toInt()},
                {QStringLiteral("confirmed"), chartQuery.value(2).toInt()}
            };
        }
    };
    appendChart(QStringLiteral("Band"), QStringLiteral("band"));
    appendChart(QStringLiteral("Mode"), QStringLiteral("mode"));
    appendChart(QStringLiteral("Continent"), QStringLiteral("continent"));

    qint64 const now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    qint64 const currentStart = now - 30LL * 24LL * 60LL * 60LL;
    qint64 const previousStart = now - 60LL * 24LL * 60LL * 60LL;
    QSqlQuery comparisonQuery(db);
    comparisonQuery.prepare(QStringLiteral(
        "SELECT"
        " sum(CASE WHEN qso_epoch>=? THEN 1 ELSE 0 END),"
        " sum(CASE WHEN qso_epoch>=? AND qso_epoch<? THEN 1 ELSE 0 END),"
        " count(DISTINCT CASE WHEN qso_epoch>=? THEN call END),"
        " count(DISTINCT CASE WHEN qso_epoch>=? AND qso_epoch<? THEN call END)"
        " FROM map_qso"));
    comparisonQuery.addBindValue(currentStart);
    comparisonQuery.addBindValue(previousStart);
    comparisonQuery.addBindValue(currentStart);
    comparisonQuery.addBindValue(currentStart);
    comparisonQuery.addBindValue(previousStart);
    comparisonQuery.addBindValue(currentStart);
    if (comparisonQuery.exec() && comparisonQuery.next()) {
        int const current = comparisonQuery.value(0).toInt();
        int const previous = comparisonQuery.value(1).toInt();
        int const currentCalls = comparisonQuery.value(2).toInt();
        int const previousCalls = comparisonQuery.value(3).toInt();
        snapshot.comparison = {
            {QStringLiteral("period"), QStringLiteral("30 days")},
            {QStringLiteral("currentQsos"), current},
            {QStringLiteral("previousQsos"), previous},
            {QStringLiteral("qsoDelta"), current - previous},
            {QStringLiteral("currentCalls"), currentCalls},
            {QStringLiteral("previousCalls"), previousCalls},
            {QStringLiteral("callDelta"), currentCalls - previousCalls}
        };
    }

    QSqlQuery markerQuery(db);
    QString const markerSql = QStringLiteral(
        "SELECT call,grid,"
        " coalesce(nullif(pota_ref,''),nullif(iota,''),nullif(wpx,'')),"
        " CASE WHEN pota_ref<>'' THEN 'POTA'"
        "      WHEN iota<>'' THEN 'IOTA' ELSE 'WPX' END,"
        " max(qso_epoch),max(confirmed)"
        " FROM map_qso WHERE %1"
        " AND (pota_ref<>'' OR iota<>'' OR wpx<>'')"
        " AND length(grid)>=4 GROUP BY 3,4 LIMIT 3000").arg(filter.where);
    if (prepareAndBind(&markerQuery, markerSql, filter.binds, nullptr)) {
        while (markerQuery.next()) {
            QString const grid = markerQuery.value(1).toString();
            QPointF const center = maidenheadCenter(grid);
            if (center.isNull()) continue;
            QString const reference = markerQuery.value(2).toString();
            QString const type = markerQuery.value(3).toString();
            snapshot.markers << QVariantMap {
                {QStringLiteral("id"),
                 QStringLiteral("%1:%2").arg(type.toLower(), reference)},
                {QStringLiteral("type"), type},
                {QStringLiteral("reference"), reference},
                {QStringLiteral("call"), markerQuery.value(0).toString()},
                {QStringLiteral("grid"), grid},
                {QStringLiteral("label"), reference},
                {QStringLiteral("longitude"), center.x()},
                {QStringLiteral("latitude"), center.y()},
                {QStringLiteral("confirmed"), markerQuery.value(5).toBool()},
                {QStringLiteral("color"),
                 type == QStringLiteral("IOTA") ? QStringLiteral("#44d7e8")
                 : type == QStringLiteral("WPX") ? QStringLiteral("#f0b94d")
                                                  : QStringLiteral("#74d66a")}
            };
        }
    }
    return snapshot;
}

void MapOperationsService::rebuildOperationalMarkers()
{
    QVariantList markers;
    if (m_layerModel && m_layerModel->layerEnabled(QStringLiteral("pota"))) {
        markers.append(m_potaMarkers);
    }

    QHash<QString, QVariantMap> loggedIota;
    for (QVariant const& value : std::as_const(m_databaseMarkers)) {
        QVariantMap const marker = value.toMap();
        if (marker.value(QStringLiteral("type")).toString()
                .compare(QStringLiteral("IOTA"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        QString const reference =
            marker.value(QStringLiteral("reference")).toString().trimmed().toUpper();
        if (!reference.isEmpty()) {
            loggedIota.insert(reference, marker);
        }
    }

    bool const iotaEnabled =
        m_layerModel && m_layerModel->layerEnabled(QStringLiteral("iota"));
    if (iotaEnabled && !m_iotaCatalogMarkers.isEmpty()) {
        for (QVariant const& value : std::as_const(m_iotaCatalogMarkers)) {
            QVariantMap marker = value.toMap();
            QString const reference =
                marker.value(QStringLiteral("reference")).toString().toUpper();
            auto const logged = loggedIota.constFind(reference);
            if (logged != loggedIota.constEnd()) {
                QVariantMap const qsoMarker = logged.value();
                for (QString const& key : {
                         QStringLiteral("call"), QStringLiteral("grid"),
                         QStringLiteral("confirmed")}) {
                    if (qsoMarker.contains(key)) {
                        marker.insert(key, qsoMarker.value(key));
                    }
                }
                marker.insert(QStringLiteral("worked"), true);
                marker.insert(
                    QStringLiteral("comments"),
                    marker.value(QStringLiteral("confirmed")).toBool()
                        ? QStringLiteral("Worked and confirmed in the imported ADIF log")
                        : QStringLiteral("Worked in the imported ADIF log"));
            }
            markers << marker;
        }
    }

    for (QVariant const& value : std::as_const(m_databaseMarkers)) {
        QVariantMap const marker = value.toMap();
        QString const type = marker.value(QStringLiteral("type")).toString().toLower();
        if (type == QStringLiteral("iota")
            && iotaEnabled && !m_iotaCatalogMarkers.isEmpty()) {
            continue;
        }
        if (!m_layerModel || m_layerModel->layerEnabled(type)) {
            markers << marker;
        }
    }
    if (m_layerModel) {
        int iotaCount = 0;
        int wpxCount = 0;
        for (QVariant const& value : std::as_const(m_databaseMarkers)) {
            QString const type =
                value.toMap().value(QStringLiteral("type")).toString().toUpper();
            if (type == QStringLiteral("IOTA")) ++iotaCount;
            if (type == QStringLiteral("WPX")) ++wpxCount;
        }
        m_layerModel->setCount(
            QStringLiteral("iota"),
            m_iotaCatalogMarkers.isEmpty()
                ? iotaCount : m_iotaCatalogMarkers.size());
        m_layerModel->setCount(QStringLiteral("wpx"), wpxCount);
    }
    if (m_operationalMarkers == markers) return;
    m_operationalMarkers = markers;
    emit operationalMarkersChanged();
}

QString MapOperationsService::normalizedLocalPath(const QString& path)
{
    QUrl const url(path);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return path;
}

bool MapOperationsService::exportLogbook(const QString& path,
                                         const QString& format)
{
    QString const localPath = normalizedLocalPath(path).trimmed();
    if (localPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Choose an export file"));
        return false;
    }
    if (m_exportInProgress) {
        setStatusMessage(QStringLiteral("A logbook export is already running"));
        return false;
    }

    QString const databasePath = m_databasePath;
    QString const search = m_logbookSearch;
    QString const band = m_logbookBand;
    QString const mode = m_logbookMode;
    QString const period = m_logbookPeriod;
    QString const sort = m_logbookSort;
    bool const descending = m_logbookSortDescending;
    setExportInProgress(true);
    setStatusMessage(QStringLiteral("Exporting the filtered logbook..."));

    auto* watcher = new QFutureWatcher<ExportResult>(this);
    connect(watcher, &QFutureWatcher<ExportResult>::finished, this,
            [this, watcher] {
        ExportResult const result = watcher->result();
        watcher->deleteLater();
        setExportInProgress(false);
        if (!result.error.isEmpty()) {
            setStatusMessage(QStringLiteral("Logbook export: %1")
                                 .arg(result.error));
            return;
        }
        m_lastExportPath = result.path;
        emit lastExportPathChanged();
        setStatusMessage(QStringLiteral("Exported %1 logbook rows")
                             .arg(result.rows));
    });
    watcher->setFuture(QtConcurrent::run(
        [databasePath, localPath, format, search, band, mode, period, sort,
         descending] {
        return exportLogbookDatabase(databasePath, localPath, format,
                                     search, band, mode, period, sort,
                                     descending);
    }));
    return true;
}

MapOperationsService::ExportResult
MapOperationsService::exportLogbookDatabase(
    const QString& databasePath, const QString& path,
    const QString& format, const QString& search,
    const QString& band, const QString& mode, const QString& period,
    const QString& sort, bool descending)
{
    ExportResult result;
    result.path = path;
    QFileInfo const info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        result.error = QStringLiteral("Cannot create export directory");
        return result;
    }

    ScopedMapDatabase connection(databasePath);
    QSqlDatabase& db = connection.database();
    if (!db.open()) {
        result.error = db.lastError().text();
        return result;
    }
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=3000"));
    SqlFilter const filter = buildFilter(search, band, mode, period);
    QString const selectSql = QStringLiteral(
        "SELECT source_key,call,grid,band,mode,qso_date,time_on,qso_epoch,"
        " frequency_mhz,confirmed,dxcc,continent,state,pota_ref,iota,wpx,source"
        " FROM map_qso WHERE %1 ORDER BY %2 %3")
        .arg(filter.where, sortableColumn(sort),
             descending ? QStringLiteral("DESC") : QStringLiteral("ASC"));
    QSqlQuery query(db);
    if (!prepareAndBind(&query, selectSql, filter.binds, &result.error)) {
        return result;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.error = file.errorString();
        return result;
    }
    QTextStream stream(&file);
    bool const adif =
        format.compare(QStringLiteral("ADIF"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("adi"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("adif"), Qt::CaseInsensitive) == 0;
    if (adif) {
        stream << "<ADIF_VER:5>3.1.4 <PROGRAMID:9>Decodium4 <EOH>\n";
    } else {
        stream << "Date,Time,Call,Grid,Band,Mode,Frequency MHz,Confirmed,DXCC,"
                  "Continent,State,POTA,IOTA,WPX,Source\n";
    }

    while (query.next()) {
        QVariantMap const row = rowToMap(query);
        if (adif) {
            auto field = [&](QString const& key, QString const& name) {
                QString const text = row.value(key).toString();
                if (!text.isEmpty()) {
                    stream << '<' << name << ':' << text.toUtf8().size()
                           << '>' << text << ' ';
                }
            };
            field(QStringLiteral("call"), QStringLiteral("CALL"));
            field(QStringLiteral("grid"), QStringLiteral("GRIDSQUARE"));
            field(QStringLiteral("band"), QStringLiteral("BAND"));
            field(QStringLiteral("mode"), QStringLiteral("MODE"));
            field(QStringLiteral("date"), QStringLiteral("QSO_DATE"));
            field(QStringLiteral("time"), QStringLiteral("TIME_ON"));
            field(QStringLiteral("pota"), QStringLiteral("POTA_REF"));
            field(QStringLiteral("iota"), QStringLiteral("IOTA"));
            stream << "<EOR>\n";
        } else {
            QStringList values {
                row.value(QStringLiteral("date")).toString(),
                row.value(QStringLiteral("time")).toString(),
                row.value(QStringLiteral("call")).toString(),
                row.value(QStringLiteral("grid")).toString(),
                row.value(QStringLiteral("band")).toString(),
                row.value(QStringLiteral("mode")).toString(),
                QString::number(
                    row.value(QStringLiteral("frequencyMhz")).toDouble(),
                    'f', 6),
                row.value(QStringLiteral("confirmed")).toBool()
                    ? QStringLiteral("Y") : QStringLiteral("N"),
                row.value(QStringLiteral("dxcc")).toString(),
                row.value(QStringLiteral("continent")).toString(),
                row.value(QStringLiteral("state")).toString(),
                row.value(QStringLiteral("pota")).toString(),
                row.value(QStringLiteral("iota")).toString(),
                row.value(QStringLiteral("wpx")).toString(),
                row.value(QStringLiteral("source")).toString()
            };
            for (QString& value : values) value = csvQuoted(value);
            stream << values.join(QLatin1Char(',')) << '\n';
        }
        ++result.rows;
    }

    if (!file.commit()) {
        result.error = file.errorString();
    }
    return result;
}

void MapOperationsService::cycleDataView()
{
    QStringList const values = availableDataViews();
    int const index = values.indexOf(m_dataViewMode);
    setDataViewMode(values.at((index + 1) % values.size()));
}

void MapOperationsService::applyMapPreset(const QString& name)
{
    QString normalized = name.trimmed();
    if (normalized.isEmpty() || !m_layerModel) return;

    QVariantMap preset;
    if (normalized.compare(QStringLiteral("Operational"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live + Logbook"),
                           {QStringLiteral("live"), QStringLiteral("active"),
                            QStringLiteral("psk"), QStringLiteral("offline")});
        normalized = QStringLiteral("Operational");
    } else if (normalized.compare(QStringLiteral("Logbook"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Logbook"),
                           {QStringLiteral("worked"), QStringLiteral("confirmed"),
                            QStringLiteral("states"), QStringLiteral("offline")});
        normalized = QStringLiteral("Logbook");
    } else if (normalized.compare(QStringLiteral("Parks"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Mercator"),
                           QStringLiteral("Live + Logbook"),
                           {QStringLiteral("live"), QStringLiteral("pota"),
                            QStringLiteral("states"), QStringLiteral("offline")});
        normalized = QStringLiteral("Parks");
    } else if (normalized.compare(QStringLiteral("Awards"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Miller"),
                           QStringLiteral("Logbook"),
                           {QStringLiteral("worked"), QStringLiteral("confirmed"),
                            QStringLiteral("iota"), QStringLiteral("wpx"),
                            QStringLiteral("offline")});
        normalized = QStringLiteral("Awards");
    } else if (normalized.compare(QStringLiteral("Propagation"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live"),
                           {QStringLiteral("live"), QStringLiteral("propagation"),
                            QStringLiteral("muf"), QStringLiteral("fof2"),
                            QStringLiteral("aurora"), QStringLiteral("offline")});
        normalized = QStringLiteral("Propagation");
    } else if (normalized.compare(QStringLiteral("Minimal"), Qt::CaseInsensitive) == 0) {
        preset = presetMap(QStringLiteral("Equirectangular"),
                           QStringLiteral("Live"),
                           {QStringLiteral("live"), QStringLiteral("offline")});
        normalized = QStringLiteral("Minimal");
    } else {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        settings.beginGroup(QStringLiteral("MapPresets"));
        settings.beginGroup(normalized);
        preset = presetMap(
            settings.value(QStringLiteral("Projection"),
                           QStringLiteral("Equirectangular")).toString(),
            settings.value(QStringLiteral("DataView"),
                           QStringLiteral("Live + Logbook")).toString(),
            settings.value(QStringLiteral("Layers")).toStringList());
        settings.endGroup();
        settings.endGroup();
    }
    QStringList const enabled = preset.value(QStringLiteral("layers")).toStringList();
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        QString const id =
            m_layerModel->data(index, MapLayerModel::LayerIdRole).toString();
        m_layerModel->setLayerEnabled(id, enabled.contains(id));
    }
    setMapProjection(preset.value(QStringLiteral("projection")).toString());
    setDataViewMode(preset.value(QStringLiteral("dataView")).toString());
    m_activeMapPreset = normalized;
    saveSetting(QStringLiteral("ActivePreset"), normalized);
    emit activeMapPresetChanged();
    refreshPota();
    refreshGeographicFeatures();
}

void MapOperationsService::saveMapPreset(const QString& name)
{
    QString const normalized = name.trimmed().left(48);
    if (normalized.isEmpty() || !m_layerModel) return;
    QStringList enabled;
    for (int row = 0; row < m_layerModel->rowCount(); ++row) {
        QModelIndex const index = m_layerModel->index(row, 0);
        if (m_layerModel->data(index, MapLayerModel::LayerEnabledRole).toBool()) {
            enabled << m_layerModel->data(index, MapLayerModel::LayerIdRole).toString();
        }
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    settings.beginGroup(normalized);
    settings.setValue(QStringLiteral("Projection"), m_mapProjection);
    settings.setValue(QStringLiteral("DataView"), m_dataViewMode);
    settings.setValue(QStringLiteral("Layers"), enabled);
    settings.endGroup();
    settings.endGroup();
    m_activeMapPreset = normalized;
    saveSetting(QStringLiteral("ActivePreset"), normalized);
    loadMapPresets();
    emit activeMapPresetChanged();
}

void MapOperationsService::deleteMapPreset(const QString& name)
{
    static const QStringList builtIns {
        QStringLiteral("Operational"), QStringLiteral("Logbook"),
        QStringLiteral("Parks"), QStringLiteral("Awards"),
        QStringLiteral("Propagation"), QStringLiteral("Minimal")
    };
    QString const normalized = name.trimmed();
    if (builtIns.contains(normalized, Qt::CaseInsensitive)) {
        setStatusMessage(QStringLiteral("Built-in presets cannot be deleted"));
        return;
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("MapPresets"));
    settings.remove(normalized);
    settings.endGroup();
    if (m_activeMapPreset == normalized) {
        m_activeMapPreset.clear();
        emit activeMapPresetChanged();
    }
    loadMapPresets();
}

void MapOperationsService::aimRotator(double azimuth)
{
    if (!m_rotatorEnabled) {
        setRotatorStatus(QStringLiteral("Enable PSTRotator first"));
        return;
    }
    if (!qIsFinite(azimuth)) return;
    int const heading = qRound(std::fmod(azimuth + 360.0, 360.0));
    QByteArray const payload =
        QStringLiteral("<PST><AZIMUTH>%1</AZIMUTH></PST>")
            .arg(heading).toUtf8();
    QHostAddress address;
    if (!address.setAddress(m_rotatorHost)) {
        address = QHostAddress::LocalHost;
    }
    qint64 const written =
        m_rotatorSocket->writeDatagram(payload, address,
                                       static_cast<quint16>(m_rotatorPort));
    setRotatorStatus(written == payload.size()
        ? QStringLiteral("PSTRotator: %1 deg").arg(heading)
        : QStringLiteral("PSTRotator send failed"));
}

void MapOperationsService::aimRotatorAt(double latitude, double longitude,
                                        double homeLatitude, double homeLongitude)
{
    aimRotator(initialBearing(homeLatitude, homeLongitude,
                              latitude, longitude));
}

QString MapOperationsService::reserveScreenshotPath()
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::PicturesLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
    }
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    QString const path = directory.absoluteFilePath(
        QStringLiteral("Decodium-Map-%1.png")
            .arg(QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd-HHmmss"))));
    emit screenshotPathReserved(path);
    return path;
}

QString MapOperationsService::reserveLogbookExportPath(const QString& format)
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::HomeLocation);
    }
    QDir directory(root);
    directory.mkpath(QStringLiteral("Decodium"));
    bool const adif =
        format.compare(QStringLiteral("ADIF"), Qt::CaseInsensitive) == 0;
    return directory.absoluteFilePath(
        QStringLiteral("Decodium-Logbook-%1.%2")
            .arg(QDateTime::currentDateTimeUtc().toString(
                     QStringLiteral("yyyyMMdd-HHmmss")),
                 adif ? QStringLiteral("adi") : QStringLiteral("csv")));
}

void MapOperationsService::setLogbookLoading(bool loading)
{
    if (m_logbookLoading == loading) return;
    m_logbookLoading = loading;
    emit logbookLoadingChanged();
}

void MapOperationsService::setGeographicLoading(bool loading)
{
    if (m_geographicLoading == loading) return;
    m_geographicLoading = loading;
    emit geographicLoadingChanged();
}

void MapOperationsService::setPotaLoading(bool loading)
{
    if (m_potaLoading == loading) return;
    m_potaLoading = loading;
    emit potaLoadingChanged();
}

void MapOperationsService::setExportInProgress(bool inProgress)
{
    if (m_exportInProgress == inProgress) return;
    m_exportInProgress = inProgress;
    emit exportInProgressChanged();
}

void MapOperationsService::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void MapOperationsService::setRotatorStatus(const QString& message)
{
    if (m_rotatorStatus == message) return;
    m_rotatorStatus = message;
    emit rotatorStatusChanged();
}
