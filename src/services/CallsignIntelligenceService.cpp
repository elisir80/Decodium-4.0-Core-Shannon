#include "CallsignIntelligenceService.h"

#include "DxccLookup.h"
#include "DecodiumProfileSettings.h"
#include "SecureSettings.hpp"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QProcess>
#include <utility>

namespace {

QString normHeader(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral("[^a-z0-9]+")));
    return value;
}

QString fieldFrom(const QStringList& headers,
                  const QStringList& row,
                  const QStringList& aliases)
{
    for (int i = 0; i < headers.size() && i < row.size(); ++i) {
        if (aliases.contains(normHeader(headers.at(i)))) {
            return row.at(i).trimmed();
        }
    }
    return {};
}

QStringList splitLine(const QString& line, QChar delimiter)
{
    QStringList result;
    QString current;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == ch) {
                current += ch;
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == delimiter && !quoted) {
            result.append(current.trimmed());
            current.clear();
        } else {
            current += ch;
        }
    }
    result.append(current.trimmed());
    return result;
}

QChar delimiterFor(const QString& line)
{
    const QList<QPair<QChar, int>> candidates {
        {QLatin1Char('|'), line.count(QLatin1Char('|'))},
        {QLatin1Char(','), line.count(QLatin1Char(','))},
        {QLatin1Char(';'), line.count(QLatin1Char(';'))},
        {QLatin1Char('\t'), line.count(QLatin1Char('\t'))}
    };
    QChar best = QLatin1Char(',');
    int count = 0;
    for (const auto& candidate : candidates) {
        if (candidate.second > count) {
            best = candidate.first;
            count = candidate.second;
        }
    }
    return best;
}

bool truthy(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("1")
        || normalized == QStringLiteral("y")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("ag")
        || normalized == QStringLiteral("ok");
}

QString jsonString(const QVariantMap& map)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
}

QString firstNonEmpty(const QVariantMap& value, const QString& key)
{
    return value.value(key).toString().trimmed();
}

} // namespace

CallsignIntelligenceService::CallsignIntelligenceService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_database(new QSqlDatabase)
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    m_databasePath = QDir(appData).absoluteFilePath(QStringLiteral("callsign-intelligence.sqlite"));
    m_connectionName = QStringLiteral("decodium_callsign_intelligence_%1")
                           .arg(reinterpret_cast<quintptr>(this));
    *m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database->setDatabaseName(m_databasePath);

    m_specs.insert(QStringLiteral("fcc_uls"), {QStringLiteral("fcc_uls"), QStringLiteral("FCC ULS"),
                                                QStringLiteral("https://data.fcc.gov/download/pub/uls/complete/l_amat.zip"), true, true});
    m_specs.insert(QStringLiteral("lotw"), {QStringLiteral("lotw"), QStringLiteral("LoTW"),
                                             QStringLiteral("https://lotw.arrl.org/lotw-user-activity.csv"), true, true});
    m_specs.insert(QStringLiteral("eqsl"), {QStringLiteral("eqsl"), QStringLiteral("eQSL AG"),
                                             QStringLiteral("https://eqsl.cc/DownloadedFiles/eQSLMemberList.csv"), true, true});
    m_specs.insert(QStringLiteral("clublog_oqrs"), {QStringLiteral("clublog_oqrs"), QStringLiteral("Club Log OQRS"),
                                                     QStringLiteral("https://clublog.org/getoqrsmatches.php"), true, true});
    m_specs.insert(QStringLiteral("dxcc"), {QStringLiteral("dxcc"), QStringLiteral("DXCC cty.dat"), {}, true, false});

    openDatabase();
    loadSettings();
    setStatus(tr("Pronto: database locale callsign disponibile"));
}

CallsignIntelligenceService::~CallsignIntelligenceService()
{
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    if (m_database && m_database->isOpen()) {
        m_database->close();
    }
    if (!m_connectionName.isEmpty()) {
        // Drop the last QSqlDatabase handle before removing the named
        // connection; this avoids Qt's "connection is still in use" warning
        // during application shutdown.
        if (m_database)
            *m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    delete m_database;
}

bool CallsignIntelligenceService::openDatabase()
{
    if (!m_database || !m_database->open()) {
        setStatus(tr("Database callsign non disponibile: %1").arg(m_database ? m_database->lastError().text() : QString()));
        return false;
    }
    createSchema();
    return true;
}

void CallsignIntelligenceService::createSchema()
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_records ("
                             "provider TEXT NOT NULL, callsign TEXT NOT NULL, grid TEXT, name TEXT, qth TEXT, "
                             "country TEXT, dxcc TEXT, continent TEXT, state TEXT, county TEXT, "
                             "last_upload TEXT, lotw INTEGER NOT NULL DEFAULT 0, eqsl INTEGER NOT NULL DEFAULT 0, "
                             "oqrs INTEGER NOT NULL DEFAULT 0, confirmed INTEGER NOT NULL DEFAULT 0, "
                             "metadata_json TEXT, updated_at INTEGER NOT NULL, PRIMARY KEY(provider,callsign))"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS callsign_records_call ON callsign_records(callsign)"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_cache ("
                             "callsign TEXT PRIMARY KEY, provider TEXT, payload_json TEXT NOT NULL, "
                             "updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL)"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_provider_state ("
                             "provider TEXT PRIMARY KEY, source_url TEXT, local_path TEXT, row_count INTEGER NOT NULL DEFAULT 0, "
                             "updated_at INTEGER NOT NULL DEFAULT 0, status TEXT, error TEXT)"));
    for (const ProviderSpec& spec : std::as_const(m_specs)) {
        QSqlQuery insert(*m_database);
        insert.prepare(QStringLiteral("INSERT OR IGNORE INTO callsign_provider_state(provider,source_url,status) VALUES(?,?,?)"));
        insert.addBindValue(spec.id);
        insert.addBindValue(spec.url);
        insert.addBindValue(spec.updateable ? QStringLiteral("Mai aggiornato") : QStringLiteral("Locale"));
        insert.exec();
    }
}

void CallsignIntelligenceService::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CallsignIntelligence"));
    m_autoOpenOnQsoStart = settings.value(QStringLiteral("AutoOpenOnQsoStart"), false).toBool();
    m_autoCloseAfterLogging = settings.value(QStringLiteral("AutoCloseAfterLogging"), false).toBool();
    m_enrichMissingFields = settings.value(QStringLiteral("EnrichMissingFields"), false).toBool();
    m_cacheTtlMinutes = qBound(5, settings.value(QStringLiteral("CacheTtlMinutes"), 1440).toInt(), 10080);
    m_clubLogEmail = settings.value(QStringLiteral("ClubLogEmail")).toString();
    const QString secureService = secure_settings::service(QStringLiteral("CALLSIGN_INTELLIGENCE"));
    m_clubLogApiKey = secure_settings::load_or_import(&settings, secureService,
                                                       QStringLiteral("ClubLogApiKey"),
                                                       settings.value(QStringLiteral("ClubLogApiKey")).toString()).trimmed();
    m_clubLogApplicationPassword = secure_settings::load_or_import(&settings, secureService,
                                                                     QStringLiteral("ClubLogApplicationPassword"),
                                                                     settings.value(QStringLiteral("ClubLogApplicationPassword")).toString()).trimmed();
    settings.endGroup();
}

void CallsignIntelligenceService::saveSetting(const QString& key, const QVariant& value)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CallsignIntelligence"));
    if (key == QStringLiteral("ClubLogApiKey") || key == QStringLiteral("ClubLogApplicationPassword")) {
        settings.setValue(key, secure_settings::value_for_write(secure_settings::service(QStringLiteral("CALLSIGN_INTELLIGENCE")), key, value.toString()));
    } else {
        settings.setValue(key, value);
    }
    settings.sync();
    settings.endGroup();
}

void CallsignIntelligenceService::setAutoOpenOnQsoStart(bool value)
{
    if (m_autoOpenOnQsoStart == value) return;
    m_autoOpenOnQsoStart = value;
    saveSetting(QStringLiteral("AutoOpenOnQsoStart"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setAutoCloseAfterLogging(bool value)
{
    if (m_autoCloseAfterLogging == value) return;
    m_autoCloseAfterLogging = value;
    saveSetting(QStringLiteral("AutoCloseAfterLogging"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setEnrichMissingFields(bool value)
{
    if (m_enrichMissingFields == value) return;
    m_enrichMissingFields = value;
    saveSetting(QStringLiteral("EnrichMissingFields"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setCacheTtlMinutes(int value)
{
    const int bounded = qBound(5, value, 10080);
    if (m_cacheTtlMinutes == bounded) return;
    m_cacheTtlMinutes = bounded;
    saveSetting(QStringLiteral("CacheTtlMinutes"), bounded);
    emit settingsChanged();
}

void CallsignIntelligenceService::setOperatorCallsign(const QString& value)
{
    const QString clean = value.trimmed().toUpper();
    if (m_operatorCallsign == clean) return;
    m_operatorCallsign = clean;
    emit settingsChanged();
}

void CallsignIntelligenceService::setClubLogApiKey(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogApiKey == clean) return;
    m_clubLogApiKey = clean;
    saveSetting(QStringLiteral("ClubLogApiKey"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setClubLogEmail(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogEmail == clean) return;
    m_clubLogEmail = clean;
    saveSetting(QStringLiteral("ClubLogEmail"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setClubLogApplicationPassword(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogApplicationPassword == clean) return;
    m_clubLogApplicationPassword = clean;
    saveSetting(QStringLiteral("ClubLogApplicationPassword"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setDxccLookup(DxccLookup* lookup)
{
    m_dxccLookup = lookup;
}

QString CallsignIntelligenceService::normalizeCall(const QString& value) const
{
    QString result = value.trimmed().toUpper();
    if (result.startsWith(QStringLiteral("CQ "))) result = result.mid(3).trimmed();
    if (result.size() > 32 || result.contains(QRegularExpression(QStringLiteral("[^A-Z0-9/ -]")))) return {};
    return result;
}

void CallsignIntelligenceService::setStatus(const QString& value)
{
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
}

void CallsignIntelligenceService::setPending(bool value)
{
    if (m_lookupPending == value) return;
    m_lookupPending = value;
    emit lookupPendingChanged();
}

QVariantMap CallsignIntelligenceService::mergeRecord(QVariantMap target, const QVariantMap& source) const
{
    static const QStringList fields {
        QStringLiteral("grid"), QStringLiteral("name"), QStringLiteral("qth"), QStringLiteral("country"),
        QStringLiteral("dxcc"), QStringLiteral("continent"), QStringLiteral("state"), QStringLiteral("county"),
        QStringLiteral("lastUpload")
    };
    for (const QString& key : fields) {
        if (firstNonEmpty(target, key).isEmpty() && !firstNonEmpty(source, key).isEmpty()) {
            target.insert(key, source.value(key));
        }
    }
    for (const QString& key : {QStringLiteral("lotw"), QStringLiteral("eqsl"), QStringLiteral("oqrs"), QStringLiteral("confirmed")}) {
        if (source.value(key).toBool()) target.insert(key, true);
    }
    QStringList providers = target.value(QStringLiteral("providers")).toStringList();
    const QString provider = source.value(QStringLiteral("provider")).toString();
    if (!provider.isEmpty() && !providers.contains(provider)) providers.append(provider);
    target.insert(QStringLiteral("providers"), providers);
    return target;
}

QVariantMap CallsignIntelligenceService::localLookup(const QString& callsign) const
{
    QVariantMap result;
    if (!m_database || !m_database->isOpen()) return result;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT provider,callsign,grid,name,qth,country,dxcc,continent,state,county,last_upload,lotw,eqsl,oqrs,confirmed,metadata_json "
                                 "FROM callsign_records WHERE callsign=? ORDER BY CASE provider WHEN 'fcc_uls' THEN 1 WHEN 'eqsl' THEN 2 WHEN 'lotw' THEN 3 WHEN 'clublog_oqrs' THEN 4 ELSE 9 END"));
    query.addBindValue(callsign);
    if (!query.exec()) return result;
    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("provider"), query.value(0).toString());
        row.insert(QStringLiteral("call"), query.value(1).toString());
        row.insert(QStringLiteral("grid"), query.value(2).toString());
        row.insert(QStringLiteral("name"), query.value(3).toString());
        row.insert(QStringLiteral("qth"), query.value(4).toString());
        row.insert(QStringLiteral("country"), query.value(5).toString());
        row.insert(QStringLiteral("dxcc"), query.value(6).toString());
        row.insert(QStringLiteral("continent"), query.value(7).toString());
        row.insert(QStringLiteral("state"), query.value(8).toString());
        row.insert(QStringLiteral("county"), query.value(9).toString());
        row.insert(QStringLiteral("lastUpload"), query.value(10).toString());
        row.insert(QStringLiteral("lotw"), query.value(11).toBool());
        row.insert(QStringLiteral("eqsl"), query.value(12).toBool());
        row.insert(QStringLiteral("oqrs"), query.value(13).toBool());
        row.insert(QStringLiteral("confirmed"), query.value(14).toBool());
        result = mergeRecord(result, row);
    }
    if (!result.isEmpty()) {
        result.insert(QStringLiteral("call"), callsign);
        result.insert(QStringLiteral("provider"), result.value(QStringLiteral("providers")).toStringList().join(QStringLiteral(", ")));
    }
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity entity = m_dxccLookup->lookup(callsign);
        if (entity.isValid()) {
            if (firstNonEmpty(result, QStringLiteral("dxcc")).isEmpty()) result.insert(QStringLiteral("dxcc"), entity.name);
            if (firstNonEmpty(result, QStringLiteral("country")).isEmpty()) result.insert(QStringLiteral("country"), entity.name);
            if (firstNonEmpty(result, QStringLiteral("continent")).isEmpty()) result.insert(QStringLiteral("continent"), entity.continent);
            result.insert(QStringLiteral("cqZone"), entity.cqZone);
            result.insert(QStringLiteral("ituZone"), entity.ituZone);
        }
    }
    return result;
}

QVariantMap CallsignIntelligenceService::cachedLookup(const QString& callsign) const
{
    QVariantMap empty;
    if (!m_database || !m_database->isOpen()) return empty;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT payload_json,expires_at FROM callsign_cache WHERE callsign=?"));
    query.addBindValue(callsign);
    if (!query.exec() || !query.next()) return empty;
    if (query.value(1).toLongLong() < QDateTime::currentMSecsSinceEpoch()) return empty;
    const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    return document.isObject() ? document.object().toVariantMap() : empty;
}

void CallsignIntelligenceService::cacheResult(const QVariantMap& value)
{
    if (!m_database || !m_database->isOpen() || value.value(QStringLiteral("call")).toString().isEmpty()) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO callsign_cache(callsign,provider,payload_json,updated_at,expires_at) VALUES(?,?,?,?,?)"));
    query.addBindValue(value.value(QStringLiteral("call")).toString());
    query.addBindValue(value.value(QStringLiteral("provider")).toString());
    query.addBindValue(jsonString(value));
    query.addBindValue(now);
    query.addBindValue(now + static_cast<qint64>(m_cacheTtlMinutes) * 60000);
    query.exec();
}

void CallsignIntelligenceService::finishLookup(const QVariantMap& value, bool fromCache, const QString& status)
{
    m_result = value;
    m_result.insert(QStringLiteral("cacheHit"), fromCache);
    m_result.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_currentCall = value.value(QStringLiteral("call"), m_currentCall).toString();
    setPending(false);
    setStatus(status);
    emit currentCallChanged();
    emit resultChanged();
    if (m_enrichMissingFields && !m_result.isEmpty()) {
        emit enrichmentReady(m_currentCall, m_result);
    }
}

void CallsignIntelligenceService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }
    m_offlineMode = offline;
    if (offline) {
        if (m_activeReply) {
            m_activeReply->abort();
        }
        setPending(false);
        setStatus(tr("Offline: solo cache e database callsign locali"));
    } else {
        setStatus(tr("Online: provider remoto callsign abilitati"));
    }
    emit offlineModeChanged();
}

void CallsignIntelligenceService::lookup(const QString& callsign, bool forceRefresh)
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) {
        setStatus(tr("Callsign non valido"));
        return;
    }
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    m_currentCall = call;
    emit currentCallChanged();
    if (!forceRefresh) {
        const QVariantMap cached = cachedLookup(call);
        if (!cached.isEmpty()) {
            finishLookup(cached, true, tr("Risultato da cache locale"));
            return;
        }
    }
    QVariantMap local = localLookup(call);
    if (!local.isEmpty()) {
        cacheResult(local);
        finishLookup(local, false, tr("Risultato da database locali"));
        return;
    }
    if (!m_offlineMode) {
        setPending(true);
        setStatus(tr("Nessun record locale: provo i provider remoti..."));
        if (!m_clubLogApiKey.isEmpty()) {
            lookupRemoteClubLog(call);
            return;
        }
    } else {
        setStatus(tr("Offline: nessun record remoto richiesto"));
    }
    QVariantMap fallback;
    fallback.insert(QStringLiteral("call"), call);
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity entity = m_dxccLookup->lookup(call);
        if (entity.isValid()) {
            fallback.insert(QStringLiteral("country"), entity.name);
            fallback.insert(QStringLiteral("dxcc"), entity.name);
            fallback.insert(QStringLiteral("continent"), entity.continent);
            fallback.insert(QStringLiteral("cqZone"), entity.cqZone);
            fallback.insert(QStringLiteral("ituZone"), entity.ituZone);
            fallback.insert(QStringLiteral("provider"), QStringLiteral("dxcc"));
        }
    }
    finishLookup(fallback, false, fallback.size() > 1
                 ? tr("Fallback DXCC: nessun profilo provider disponibile")
                 : tr("Nessun provider ha trovato il callsign"));
}

void CallsignIntelligenceService::lookupRemoteClubLog(const QString& callsign)
{
    if (m_offlineMode) {
        return;
    }
    QUrl url(QStringLiteral("https://clublog.org/watch.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("call"), callsign);
    query.addQueryItem(QStringLiteral("api"), m_clubLogApiKey);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
    request.setTransferTimeout(15000);
    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, callsign]() {
        handleRemoteLookupFinished(reply, callsign);
    });
}

void CallsignIntelligenceService::handleRemoteLookupFinished(QNetworkReply* reply, const QString& callsign)
{
    if (!reply) return;
    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    m_activeReply = nullptr;
    if (m_offlineMode) {
        return;
    }
    QVariantMap local = localLookup(callsign);
    if (error == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            const QJsonObject object = doc.object();
            QVariantMap remote;
            remote.insert(QStringLiteral("call"), callsign);
            remote.insert(QStringLiteral("provider"), QStringLiteral("clublog"));
            remote.insert(QStringLiteral("grid"), object.value(QStringLiteral("qra")).toString());
            remote.insert(QStringLiteral("oqrs"), object.value(QStringLiteral("has_oqrs")).toBool());
            remote.insert(QStringLiteral("clubLogUser"), object.value(QStringLiteral("clublog_user")).toBool());
            remote.insert(QStringLiteral("isExpedition"), object.value(QStringLiteral("is_expedition")).toBool());
            const QJsonObject info = object.value(QStringLiteral("clublog_info")).toObject();
            remote.insert(QStringLiteral("lastClubLogUpload"), info.value(QStringLiteral("last_clublog_upload")).toString());
            remote.insert(QStringLiteral("lastLotwConfirmation"), info.value(QStringLiteral("last_lotw_confirmation")).toString());
            remote = mergeRecord(remote, local);
            cacheResult(remote);
            finishLookup(remote, false, tr("Risultato da Club Log con fallback locale"));
            return;
        }
    }
    if (!local.isEmpty()) {
        cacheResult(local);
        finishLookup(local, false, tr("Club Log non disponibile: usato fallback locale (%1)").arg(errorText));
    } else {
        finishLookup(QVariantMap{{QStringLiteral("call"), callsign}}, false,
                     tr("Provider remoti non disponibile: %1").arg(errorText));
    }
}

bool CallsignIntelligenceService::upsertRecord(const QString& provider, const QVariantMap& record)
{
    if (!m_database || !m_database->isOpen()) return false;
    const QString call = normalizeCall(record.value(QStringLiteral("call")).toString());
    if (call.isEmpty()) return false;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO callsign_records(provider,callsign,grid,name,qth,country,dxcc,continent,state,county,last_upload,lotw,eqsl,oqrs,confirmed,metadata_json,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(provider);
    query.addBindValue(call);
    query.addBindValue(record.value(QStringLiteral("grid")).toString().trimmed().toUpper());
    query.addBindValue(record.value(QStringLiteral("name")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("qth")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("country")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("dxcc")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("continent")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("state")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("county")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("lastUpload")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("lotw")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("eqsl")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("oqrs")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("confirmed")).toBool() ? 1 : 0);
    query.addBindValue(jsonString(record));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool CallsignIntelligenceService::importDelimited(const QString& provider, const QByteArray& data)
{
    QString text = QString::fromUtf8(data);
    QTextStream stream(&text, QIODevice::ReadOnly);
    stream.setEncoding(QStringConverter::Utf8);
    const QString first = stream.readLine();
    if (first.isNull()) return false;
    const QChar delimiter = delimiterFor(first);
    QStringList headers = splitLine(first, delimiter);
    bool hasHeader = false;
    for (const QString& header : headers) {
        const QString normalized = normHeader(header);
        if (normalized == QStringLiteral("call") || normalized == QStringLiteral("callsign")
            || normalized == QStringLiteral("gridsquare") || normalized == QStringLiteral("licenseename")) {
            hasHeader = true;
            break;
        }
    }
    int imported = 0;
    QSqlDatabase db = *m_database;
    db.transaction();
    auto importRow = [this, &imported, &provider](const QStringList& row, const QStringList& effectiveHeaders) {
        QString call = fieldFrom(effectiveHeaders, row, {QStringLiteral("call"), QStringLiteral("callsign"), QStringLiteral("matchedcallsign"), QStringLiteral("username")});
        if (call.isEmpty() && !row.isEmpty()) call = row.first();
        QVariantMap record;
        record.insert(QStringLiteral("call"), call);
        record.insert(QStringLiteral("grid"), fieldFrom(effectiveHeaders, row, {QStringLiteral("grid"), QStringLiteral("gridsquare"), QStringLiteral("locator"), QStringLiteral("qra")}));
        record.insert(QStringLiteral("name"), fieldFrom(effectiveHeaders, row, {QStringLiteral("name"), QStringLiteral("fullname"), QStringLiteral("licensename"), QStringLiteral("operator")}));
        record.insert(QStringLiteral("qth"), fieldFrom(effectiveHeaders, row, {QStringLiteral("qth"), QStringLiteral("city"), QStringLiteral("address"), QStringLiteral("location")}));
        record.insert(QStringLiteral("state"), fieldFrom(effectiveHeaders, row, {QStringLiteral("state"), QStringLiteral("st"), QStringLiteral("usstate")}));
        record.insert(QStringLiteral("county"), fieldFrom(effectiveHeaders, row, {QStringLiteral("county"), QStringLiteral("countrycounty")}));
        if (provider == QStringLiteral("lotw")) {
            record.insert(QStringLiteral("lotw"), true);
            if (row.size() > 1) record.insert(QStringLiteral("lastUpload"), row.at(1));
        } else if (provider == QStringLiteral("eqsl")) {
            record.insert(QStringLiteral("eqsl"), true);
        }
        if (upsertRecord(provider, record)) ++imported;
    };
    if (hasHeader) {
        while (!stream.atEnd()) {
            const QStringList row = splitLine(stream.readLine(), delimiter);
            if (!row.isEmpty()) importRow(row, headers);
        }
    } else {
        // LoTW AG activity and eQSL AGMemberListDated are intentionally
        // headerless: first column is the callsign, second is activity date.
        headers = {QStringLiteral("CALLSIGN"), QStringLiteral("LAST_UPLOAD")};
        importRow(splitLine(first, delimiter), headers);
        while (!stream.atEnd()) importRow(splitLine(stream.readLine(), delimiter), headers);
    }
    const bool committed = db.commit();
    if (!committed) db.rollback();
    if (committed) refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), imported, tr("Aggiornato"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importAdif(const QString& provider, const QByteArray& data)
{
    const QString text = QString::fromUtf8(data);
    const QRegularExpression recordExpression(QStringLiteral("(?is)(.*?)(?:<EOR>|$)"));
    auto iterator = recordExpression.globalMatch(text);
    int imported = 0;
    QSqlDatabase db = *m_database;
    db.transaction();
    while (iterator.hasNext()) {
        const QString recordText = iterator.next().captured(1);
        QVariantMap record;
        const QRegularExpression tag(QStringLiteral("(?i)<([A-Z0-9_]+)(?::\\d+)?>([^<]*)"));
        auto tags = tag.globalMatch(recordText);
        while (tags.hasNext()) {
            const auto match = tags.next();
            const QString key = match.captured(1).toUpper();
            const QString value = match.captured(2).trimmed();
            if (key == QStringLiteral("CALL")) record.insert(QStringLiteral("call"), value);
            else if (key == QStringLiteral("GRIDSQUARE")) record.insert(QStringLiteral("grid"), value);
            else if (key == QStringLiteral("NAME")) record.insert(QStringLiteral("name"), value);
            else if (key == QStringLiteral("QTH")) record.insert(QStringLiteral("qth"), value);
            else if (key == QStringLiteral("STATE")) record.insert(QStringLiteral("state"), value);
            else if (key == QStringLiteral("COUNTY")) record.insert(QStringLiteral("county"), value);
            else if (key == QStringLiteral("LOTW_QSL_RCVD")) record.insert(QStringLiteral("lotw"), truthy(value));
            else if (key == QStringLiteral("EQSL_QSL_RCVD")) record.insert(QStringLiteral("eqsl"), truthy(value));
        }
        if (provider == QStringLiteral("lotw")) record.insert(QStringLiteral("lotw"), true);
        if (provider == QStringLiteral("eqsl")) record.insert(QStringLiteral("eqsl"), true);
        if (upsertRecord(provider, record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), imported, tr("Aggiornato"));
    return committed && imported > 0;
}

QByteArray CallsignIntelligenceService::extractFccEnDat(const QByteArray& data) const
{
    if (!data.startsWith("PK")) return data;
    // Feed unzip a real temporary archive.  Passing the ZIP bytes to stdin
    // with "unzip -p -" is not portable across the BSD/GNU unzip variants
    // shipped on the supported platforms.
    QTemporaryFile archive;
    if (!archive.open()) return {};
    if (archive.write(data) != data.size() || !archive.flush()) return {};
    QProcess process;
    process.start(QStringLiteral("unzip"), {QStringLiteral("-p"), archive.fileName(), QStringLiteral("EN.dat")});
    if (!process.waitForStarted(3000)) return {};
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return process.readAllStandardOutput();
}

bool CallsignIntelligenceService::importFcc(const QByteArray& data)
{
    const QByteArray extracted = extractFccEnDat(data);
    if (extracted.isEmpty()) return false;
    QString text = QString::fromUtf8(extracted);
    QTextStream stream(&text, QIODevice::ReadOnly);
    stream.setEncoding(QStringConverter::Utf8);
    QSqlDatabase db = *m_database;
    db.transaction();
    int imported = 0;
    const QRegularExpression callPattern(QStringLiteral("^[A-Z0-9]{1,3}[0-9][A-Z0-9]{1,5}(/[A-Z0-9]+)?$"));
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.isEmpty() || fields.first().trimmed().toUpper() != QStringLiteral("EN")) continue;
        QString call;
        for (const QString& field : fields) {
            const QString candidate = field.trimmed().toUpper();
            if (candidate.size() >= 4 && callPattern.match(candidate).hasMatch()) {
                call = candidate;
                break;
            }
        }
        if (call.isEmpty()) continue;
        QVariantMap record;
        record.insert(QStringLiteral("call"), call);
        // EN.dat is the FCC entity file.  The exact field positions have
        // changed over ULS revisions, therefore use stable semantic hints and
        // retain the raw row for diagnostics rather than hard-coding columns.
        record.insert(QStringLiteral("name"), fields.value(7).trimmed());
        record.insert(QStringLiteral("qth"), fields.value(10).trimmed());
        record.insert(QStringLiteral("state"), fields.value(11).trimmed());
        record.insert(QStringLiteral("metadata"), line);
        if (upsertRecord(QStringLiteral("fcc_uls"), record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(QStringLiteral("fcc_uls"), QDateTime::currentMSecsSinceEpoch(), imported, tr("Aggiornato"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importClubLogOqrs(const QByteArray& data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isArray()) return false;
    QSqlDatabase db = *m_database;
    db.transaction();
    int imported = 0;
    for (const QJsonValue& value : document.array()) {
        const QJsonArray row = value.toArray();
        if (row.size() < 4) continue;
        QVariantMap record;
        record.insert(QStringLiteral("call"), row.at(0).toString());
        record.insert(QStringLiteral("lastUpload"), row.at(1).toString());
        record.insert(QStringLiteral("oqrs"), true);
        record.insert(QStringLiteral("confirmed"), true);
        record.insert(QStringLiteral("metadata"), QStringLiteral("band=%1 mode=%2").arg(row.at(2).toString(), row.at(3).toString()));
        if (upsertRecord(QStringLiteral("clublog_oqrs"), record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(QStringLiteral("clublog_oqrs"), QDateTime::currentMSecsSinceEpoch(), imported, tr("Aggiornato"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importBytes(const QString& provider, const QByteArray& data, const QString& sourcePath)
{
    Q_UNUSED(sourcePath)
    if (provider == QStringLiteral("fcc_uls")) return importFcc(data);
    if (provider == QStringLiteral("clublog_oqrs") && data.trimmed().startsWith('[')) return importClubLogOqrs(data);
    const QString text = QString::fromUtf8(data);
    if (text.contains(QStringLiteral("<EOR>"), Qt::CaseInsensitive) || text.contains(QStringLiteral("<CALL:"), Qt::CaseInsensitive)) return importAdif(provider, data);
    return importDelimited(provider, data);
}

bool CallsignIntelligenceService::importDatabase(const QString& provider, const QString& path)
{
    const QString cleanProvider = provider.trimmed().toLower();
    if (!m_specs.contains(cleanProvider) || cleanProvider == QStringLiteral("dxcc")) return false;
    QString localPath = path;
    if (localPath.startsWith(QStringLiteral("file://"))) localPath = QUrl(localPath).toLocalFile();
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        refreshDatabaseState(cleanProvider, 0, 0, tr("Errore"), file.errorString());
        return false;
    }
    const bool ok = importBytes(cleanProvider, file.readAll(), localPath);
    if (ok) {
        QSqlQuery query(*m_database);
        query.prepare(QStringLiteral("UPDATE callsign_provider_state SET local_path=?,status=?,error='' WHERE provider=?"));
        query.addBindValue(QFileInfo(localPath).absoluteFilePath());
        query.addBindValue(tr("Aggiornato"));
        query.addBindValue(cleanProvider);
        query.exec();
        emit databasesChanged();
        if (!m_currentCall.isEmpty()) lookup(m_currentCall, true);
    }
    return ok;
}

void CallsignIntelligenceService::refreshDatabase(const QString& provider)
{
    const QString cleanProvider = provider.trimmed().toLower();
    if (!m_specs.contains(cleanProvider) || cleanProvider == QStringLiteral("dxcc")) return;
    if (m_offlineMode) {
        setStatus(tr("Offline: aggiornamenti remoti callsign disabilitati"));
        return;
    }
    if (m_activeReply) {
        setStatus(tr("Aggiornamento già in corso"));
        return;
    }
    if (cleanProvider == QStringLiteral("clublog_oqrs")) {
        if (m_clubLogApiKey.isEmpty() || m_clubLogEmail.isEmpty() || m_clubLogApplicationPassword.isEmpty() || m_operatorCallsign.isEmpty()) {
            setStatus(tr("Club Log OQRS: API key, email, application password e callsign operatore richiesti"));
            return;
        }
        QUrl url(QStringLiteral("https://clublog.org/getoqrsmatches.php"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("api"), m_clubLogApiKey);
        query.addQueryItem(QStringLiteral("email"), m_clubLogEmail);
        query.addQueryItem(QStringLiteral("password"), m_clubLogApplicationPassword);
        query.addQueryItem(QStringLiteral("callsign"), m_operatorCallsign);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
        request.setTransferTimeout(30000);
        m_updateProvider = cleanProvider;
        m_activeReply = m_network->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
            handleDatabaseReply(reply, cleanProvider);
        });
        setStatus(tr("Aggiornamento Club Log OQRS in corso..."));
        return;
    }
    const QUrl url(providerUrl(cleanProvider));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
    request.setTransferTimeout(60000);
    m_updateProvider = cleanProvider;
    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
        handleDatabaseReply(reply, cleanProvider);
    });
    setStatus(tr("Aggiornamento %1 in corso...").arg(providerLabel(cleanProvider)));
}

void CallsignIntelligenceService::handleDatabaseReply(QNetworkReply* reply, const QString& provider)
{
    if (!reply) return;
    const QByteArray payload = reply->readAll();
    const auto error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    m_activeReply = nullptr;
    if (m_offlineMode) {
        return;
    }
    if (error != QNetworkReply::NoError) {
        refreshDatabaseState(provider, 0, 0, tr("Errore"), errorText);
        setStatus(tr("Aggiornamento %1 fallito: %2").arg(providerLabel(provider), errorText));
        emit databasesChanged();
        return;
    }
    const bool ok = importBytes(provider, payload, QString());
    if (!ok) {
        refreshDatabaseState(provider, 0, 0, tr("Errore"), tr("Formato dati non riconosciuto o nessun record"));
        setStatus(tr("Aggiornamento %1 fallito: formato non riconosciuto").arg(providerLabel(provider)));
    } else {
        setStatus(tr("%1 aggiornato").arg(providerLabel(provider)));
    }
    emit databasesChanged();
}

void CallsignIntelligenceService::refreshDatabaseState(const QString& provider, qint64 updatedAt, int rowCount, const QString& status, const QString& error)
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("UPDATE callsign_provider_state SET updated_at=?,row_count=?,status=?,error=? WHERE provider=?"));
    query.addBindValue(updatedAt);
    query.addBindValue(rowCount);
    query.addBindValue(status);
    query.addBindValue(error);
    query.addBindValue(provider);
    query.exec();
    emit databasesChanged();
}

QVariantMap CallsignIntelligenceService::providerState(const QString& provider) const
{
    QVariantMap state;
    const ProviderSpec spec = m_specs.value(provider);
    state.insert(QStringLiteral("id"), provider);
    state.insert(QStringLiteral("label"), spec.label);
    state.insert(QStringLiteral("url"), spec.url);
    state.insert(QStringLiteral("updateable"), spec.updateable);
    if (!m_database || !m_database->isOpen()) return state;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT source_url,local_path,row_count,updated_at,status,error FROM callsign_provider_state WHERE provider=?"));
    query.addBindValue(provider);
    if (query.exec() && query.next()) {
        state.insert(QStringLiteral("url"), query.value(0).toString());
        state.insert(QStringLiteral("localPath"), query.value(1).toString());
        state.insert(QStringLiteral("rowCount"), query.value(2).toInt());
        state.insert(QStringLiteral("updatedAt"), query.value(3).toLongLong());
        state.insert(QStringLiteral("status"), query.value(4).toString());
        state.insert(QStringLiteral("error"), query.value(5).toString());
    }
    return state;
}

QVariantList CallsignIntelligenceService::databases() const
{
    QVariantList result;
    for (const QString& provider : {QStringLiteral("fcc_uls"), QStringLiteral("lotw"), QStringLiteral("eqsl"), QStringLiteral("clublog_oqrs"), QStringLiteral("dxcc")}) {
        result.append(providerState(provider));
    }
    return result;
}

QString CallsignIntelligenceService::providerUrl(const QString& provider) const
{
    return m_specs.value(provider).url;
}

QString CallsignIntelligenceService::providerLabel(const QString& provider) const
{
    return m_specs.value(provider).label.isEmpty() ? provider : m_specs.value(provider).label;
}

QString CallsignIntelligenceService::externalUrl(const QString& provider, const QString& callsign) const
{
    const QString call = QUrl::toPercentEncoding(callsign);
    if (provider == QStringLiteral("qrz")) return QStringLiteral("https://www.qrz.com/db/%1").arg(call);
    if (provider == QStringLiteral("fcc_uls")) return QStringLiteral("https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp?callSign=%1").arg(call);
    if (provider == QStringLiteral("eqsl")) return QStringLiteral("https://www.eqsl.cc/Member.cfm?%1").arg(call);
    if (provider == QStringLiteral("clublog")) return QStringLiteral("https://clublog.org/logsearch/%1").arg(call);
    return QStringLiteral("https://www.google.com/search?q=%1+amateur+radio").arg(call);
}

void CallsignIntelligenceService::openProviderLookup(const QString& provider) const
{
    if (m_offlineMode) return;
    const QString cleanProvider = provider.trimmed().toLower().isEmpty() ? QStringLiteral("qrz") : provider.trimmed().toLower();
    if (m_currentCall.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(externalUrl(cleanProvider, m_currentCall)));
}

void CallsignIntelligenceService::clearCache(const QString& callsign)
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    if (callsign.trimmed().isEmpty()) {
        query.exec(QStringLiteral("DELETE FROM callsign_cache"));
    } else {
        query.prepare(QStringLiteral("DELETE FROM callsign_cache WHERE callsign=?"));
        query.addBindValue(normalizeCall(callsign));
        query.exec();
    }
    setStatus(tr("Cache callsign svuotata"));
}

QVariantMap CallsignIntelligenceService::lookupForFields(const QString& callsign) const
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) return {};
    QVariantMap value = cachedLookup(call);
    if (value.isEmpty()) value = localLookup(call);
    return value;
}

void CallsignIntelligenceService::notifyQsoStarted(const QString& callsign)
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) return;
    lookup(call);
    if (m_autoOpenOnQsoStart) emit lookupWindowRequested();
}

void CallsignIntelligenceService::notifyQsoLogged(const QString& callsign)
{
    if (m_autoCloseAfterLogging && normalizeCall(callsign) == m_currentCall) {
        emit lookupWindowCloseRequested();
    }
}
