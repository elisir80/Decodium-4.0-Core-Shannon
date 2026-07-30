#include "MapIntelligenceService.h"

#include "DxccLookup.h"
#include "MapBaseMapService.h"
#include "MapExternalOverlayService.h"
#include "MapLayerModel.h"
#include "MapOperationsService.h"
#include "MapPskFeedService.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QTime>
#include <QTimeZone>
#include <QTimer>
#include <QUuid>
#include <QVector>

#include <algorithm>
#include <memory>
#include <utility>

namespace {

constexpr qint64 kMaxAdifBytes = 64 * 1024 * 1024;
constexpr int kMaxAdifRecords = 500000;
// Changing this invalidates the derived map_qso cache once.  The source ADI
// itself remains untouched; only metadata derived from it is rebuilt.
constexpr int kAdifImportFormatVersion = 2;
constexpr int kMaxPendingLiveSpots = 512;
constexpr int kRosterLimit = 100;
constexpr int kRosterCandidateLimit = 500;
constexpr qint64 kLiveRetentionMs = 30LL * 24LL * 60LL * 60LL * 1000LL;
// The live table is deliberately short-lived.  The event table is a separate
// historical stream used by the heatmap, timeline and path overlays.
constexpr qint64 kSpotEventRetentionMs = 30LL * 24LL * 60LL * 60LL * 1000LL;

QStringList mapCtyDatSearchPaths()
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QStringList paths {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/cty.dat"),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
            + QStringLiteral("/cty.dat"),
        appDir + QStringLiteral("/cty.dat"),
        QDir(appDir).absoluteFilePath(QStringLiteral("../cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/Decodium/cty.dat")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../share/wsjtx/cty.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("cty.dat")),
        QDir::current().absoluteFilePath(QStringLiteral("resources/runtime/cty.dat"))
    };

    QStringList uniquePaths;
    QSet<QString> seen;
    for (QString const& path : paths) {
        QString const cleanPath = QDir::cleanPath(path);
        if (!cleanPath.isEmpty() && !seen.contains(cleanPath)) {
            seen.insert(cleanPath);
            uniquePaths.append(cleanPath);
        }
    }
    return uniquePaths;
}

std::shared_ptr<const DxccLookup> adifDxccLookup()
{
    static QMutex mutex;
    static QString cachedPath;
    static QDateTime cachedModified;
    static std::shared_ptr<DxccLookup> cachedLookup;

    QString path;
    QFileInfo info;
    for (QString const& candidate : mapCtyDatSearchPaths()) {
        QFileInfo const candidateInfo(candidate);
        if (candidateInfo.isFile()) {
            path = candidateInfo.absoluteFilePath();
            info = candidateInfo;
            break;
        }
    }

    QMutexLocker locker(&mutex);
    if (path == cachedPath && info.lastModified() == cachedModified) {
        return cachedLookup;
    }

    cachedPath = path;
    cachedModified = info.lastModified();
    cachedLookup.reset();
    if (path.isEmpty()) {
        return {};
    }

    auto lookup = std::make_shared<DxccLookup>();
    if (!lookup->loadCtyDat(path)) {
        return {};
    }
    cachedLookup = std::move(lookup);
    return cachedLookup;
}

QString decodedAdifValue(const QByteArray& bytes)
{
    QString value = QString::fromUtf8(bytes);
    if (value.contains(QChar::ReplacementCharacter)) {
        value = QString::fromLocal8Bit(bytes);
    }
    return value.trimmed();
}

QString normalizedGrid(QString value)
{
    value = value.trimmed().toUpper();
    if (value.size() < 4) {
        return {};
    }
    QString const square = value.left(4);
    if (square.at(0) < QLatin1Char('A') || square.at(0) > QLatin1Char('R')
        || square.at(1) < QLatin1Char('A') || square.at(1) > QLatin1Char('R')
        || !square.at(2).isDigit() || !square.at(3).isDigit()) {
        return {};
    }
    return value.left(qMin(6, value.size()));
}

QString normalizedMode(QString mode, QString submode = {})
{
    mode = mode.trimmed().toUpper();
    submode = submode.trimmed().toUpper();
    if (!submode.isEmpty()
        && (mode == QStringLiteral("MFSK")
            || mode == QStringLiteral("JT9")
            || mode.isEmpty())) {
        return submode;
    }
    return mode;
}

QString normalizedIota(QString value)
{
    value = value.trimmed().toUpper();
    static const QRegularExpression expression(
        QStringLiteral("^[A-Z]{2}-\\d{3}$"));
    return expression.match(value).hasMatch() ? value : QString();
}

QString normalizedPota(QString value)
{
    value = value.trimmed().toUpper();
    static const QRegularExpression expression(
        QStringLiteral("^[A-Z0-9]{1,4}-\\d{1,6}$"));
    return expression.match(value).hasMatch() ? value : QString();
}

QString wpxPrefix(QString call)
{
    call = call.trimmed().toUpper();
    QStringList const parts = call.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString const& part : parts) {
        if (part.size() >= 3
            && part.contains(QRegularExpression(QStringLiteral("[A-Z]")))
            && part.contains(QRegularExpression(QStringLiteral("\\d")))) {
            call = part;
            break;
        }
    }
    call.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    int lastDigit = -1;
    for (int index = 0; index < call.size(); ++index) {
        if (call.at(index).isDigit()) {
            lastDigit = index;
            break;
        }
    }
    if (lastDigit < 0) {
        QString letters = call.left(2);
        return letters.isEmpty() ? QString() : letters + QLatin1Char('0');
    }
    return call.left(lastDigit + 1);
}

int bandOrder(const QString& band)
{
    static const QStringList ordered {
        QStringLiteral("2190m"), QStringLiteral("630m"), QStringLiteral("560m"),
        QStringLiteral("160m"), QStringLiteral("80m"), QStringLiteral("60m"),
        QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
        QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
        QStringLiteral("10m"), QStringLiteral("8m"), QStringLiteral("6m"),
        QStringLiteral("5m"), QStringLiteral("4m"), QStringLiteral("2m"),
        QStringLiteral("1.25m"), QStringLiteral("70cm"), QStringLiteral("33cm"),
        QStringLiteral("23cm"), QStringLiteral("13cm"), QStringLiteral("9cm"),
        QStringLiteral("6cm"), QStringLiteral("3cm"), QStringLiteral("1.25cm")
    };
    int const index = ordered.indexOf(band.toLower());
    return index >= 0 ? index : 1000;
}

QString bandFromFrequencyMhz(double mhz)
{
    struct Range { double low; double high; const char* name; };
    static const Range ranges[] = {
        {0.135, 0.138, "2190m"}, {0.472, 0.480, "630m"},
        {1.8, 2.0, "160m"}, {3.5, 4.1, "80m"}, {5.0, 5.6, "60m"},
        {7.0, 7.4, "40m"}, {10.0, 10.2, "30m"}, {14.0, 14.4, "20m"},
        {18.0, 18.2, "17m"}, {21.0, 21.5, "15m"}, {24.8, 25.0, "12m"},
        {28.0, 30.0, "10m"}, {40.0, 45.0, "8m"}, {50.0, 54.5, "6m"},
        {70.0, 71.0, "4m"}, {144.0, 148.0, "2m"}, {219.0, 225.0, "1.25m"},
        {420.0, 450.0, "70cm"}, {902.0, 928.0, "33cm"},
        {1240.0, 1300.0, "23cm"}, {2300.0, 2450.0, "13cm"},
        {3300.0, 3500.0, "9cm"}, {5650.0, 5925.0, "6cm"},
        {10000.0, 10500.0, "3cm"}, {24000.0, 24250.0, "1.25cm"}
    };
    for (Range const& range : ranges) {
        if (mhz >= range.low && mhz <= range.high) {
            return QString::fromLatin1(range.name);
        }
    }
    return {};
}

QString normalizedBand(QString band, double frequencyMhz)
{
    band = band.trimmed().toLower();
    return band.isEmpty() ? bandFromFrequencyMhz(frequencyMhz) : band;
}

bool isConfirmed(const QHash<QString, QString>& fields)
{
    static const QStringList confirmationFields {
        QStringLiteral("QSL_RCVD"),
        QStringLiteral("LOTW_QSL_RCVD"),
        QStringLiteral("EQSL_QSL_RCVD")
    };
    for (QString const& key : confirmationFields) {
        if (fields.value(key).trimmed().compare(QStringLiteral("Y"), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool adifYes(const QString& value)
{
    QString const normalized = value.trimmed().toUpper();
    return normalized == QStringLiteral("Y")
        || normalized == QStringLiteral("YES")
        || normalized == QStringLiteral("1");
}

struct ExternalAwardDefinition {
    QString id;
    QString label;
    QString sponsor;
    QString type;
    QString tooltip;
    QStringList bands;
    QStringList modes;
    // Some catalog rules collapse multiple prefixes or suffixes into one
    // award entity.  Preserve those equivalences instead of treating every
    // raw callsign fragment as a separate award credit.
    QVector<QStringList> aliases;
    QStringList prefixWhitelist;
    int target {0};
};

QStringList jsonStrings(const QJsonValue& value)
{
    QStringList values;
    if (value.isString()) {
        QString const text = value.toString().trimmed().toUpper();
        if (!text.isEmpty()) values.append(text);
    } else if (value.isArray()) {
        for (QJsonValue const& child : value.toArray()) {
            values.append(jsonStrings(child));
        }
    }
    values.removeDuplicates();
    return values;
}

QVector<QStringList> jsonAliasGroups(const QJsonValue& value)
{
    QVector<QStringList> groups;
    if (!value.isArray()) return groups;
    for (QJsonValue const& child : value.toArray()) {
        QStringList const group = jsonStrings(child);
        if (!group.isEmpty()) groups.append(group);
    }
    return groups;
}

QVector<ExternalAwardDefinition> const& externalAwardDefinitions()
{
    static const QVector<ExternalAwardDefinition> definitions = [] {
        QVector<ExternalAwardDefinition> parsed;
        QFile file(QStringLiteral(":/decodium-awards-catalog.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning().noquote() << "[MAPINT] award catalog unavailable:" << file.errorString();
            return parsed;
        }
        QJsonParseError error;
        QJsonDocument const document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            qWarning().noquote() << "[MAPINT] award catalog parse failed:" << error.errorString();
            return parsed;
        }
        QJsonObject const sponsors = document.object();
        for (auto sponsorIt = sponsors.constBegin(); sponsorIt != sponsors.constEnd(); ++sponsorIt) {
            QJsonObject const awards = sponsorIt.value().toObject()
                                          .value(QStringLiteral("awards")).toObject();
            for (auto awardIt = awards.constBegin(); awardIt != awards.constEnd(); ++awardIt) {
                QJsonObject const award = awardIt.value().toObject();
                QJsonObject const rule = award.value(QStringLiteral("rule")).toObject();
                if (rule.isEmpty()) continue;
                ExternalAwardDefinition definition;
                definition.sponsor = award.value(QStringLiteral("sponsor"))
                                         .toString(sponsorIt.key()).trimmed();
                definition.id = definition.sponsor + QStringLiteral(":") + awardIt.key();
                QString const name = award.value(QStringLiteral("name"))
                                         .toString(awardIt.key()).trimmed();
                definition.label = definition.sponsor + QStringLiteral(": ") + name;
                definition.type = rule.value(QStringLiteral("type")).toString().trimmed().toLower();
                definition.tooltip = award.value(QStringLiteral("tooltip")).toString().trimmed();
                for (QJsonValue const& value : rule.value(QStringLiteral("band")).toArray()) {
                    definition.bands.append(value.toString().trimmed());
                }
                for (QJsonValue const& value : rule.value(QStringLiteral("mode")).toArray()) {
                    definition.modes.append(value.toString().trimmed());
                }
                for (QJsonValue const& value : rule.value(QStringLiteral("count")).toArray()) {
                    definition.target = qMax(definition.target, value.toInt());
                }
                if (definition.type == QStringLiteral("pxa")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("pxa")));
                } else if (definition.type == QStringLiteral("numsfx")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("numsfx")));
                } else if (definition.type == QStringLiteral("sfx")) {
                    definition.aliases = jsonAliasGroups(rule.value(QStringLiteral("sfx")));
                } else if (definition.type == QStringLiteral("pxplus")) {
                    definition.prefixWhitelist = jsonStrings(rule.value(QStringLiteral("pxplus")));
                }
                if (definition.target <= 0) definition.target = 1;
                if (!definition.type.isEmpty()) parsed.append(std::move(definition));
            }
        }
        std::sort(parsed.begin(), parsed.end(), [](ExternalAwardDefinition const& left,
                                                    ExternalAwardDefinition const& right) {
            return left.label.localeAwareCompare(right.label) < 0;
        });
        return parsed;
    }();
    return definitions;
}

ExternalAwardDefinition const* externalAwardForLabel(const QString& label)
{
    for (ExternalAwardDefinition const& definition : externalAwardDefinitions()) {
        if (definition.label.compare(label, Qt::CaseInsensitive) == 0) {
            return &definition;
        }
    }
    return nullptr;
}

QString externalAwardEntityExpression(const ExternalAwardDefinition& definition)
{
    QString const type = definition.type;
    if (type == QStringLiteral("grids")) return QStringLiteral("upper(grid4)");
    if (type == QStringLiteral("dxcc")) return QStringLiteral("lower(dxcc)");
    if (type == QStringLiteral("dxcc2band")) {
        return QStringLiteral("lower(dxcc) || '@' || lower(band)");
    }
    if (type == QStringLiteral("cqz")) return QStringLiteral("CAST(cq_zone AS TEXT)");
    if (type == QStringLiteral("states")) {
        return QStringLiteral("upper(state)");
    }
    if (type == QStringLiteral("states2band")) {
        return QStringLiteral("upper(state) || '@' || lower(band)");
    }
    if (type == QStringLiteral("cnty")) return QStringLiteral("upper(county)");
    if (type == QStringLiteral("iota")) return QStringLiteral("upper(iota)");
    if (type == QStringLiteral("cont") || type == QStringLiteral("cont5")) {
        return QStringLiteral("upper(continent)");
    }
    if (type == QStringLiteral("cont2band") || type == QStringLiteral("cont52band")) {
        return QStringLiteral("upper(continent) || '@' || lower(band)");
    }
    if (type == QStringLiteral("call") || type == QStringLiteral("calls2dxcc")) {
        return QStringLiteral("upper(call)");
    }
    if (type == QStringLiteral("calls2band")) {
        return QStringLiteral("upper(call) || '@' || lower(band)");
    }
    // Prefix and call-area rules use the normalized WPX prefix stored during
    // ADIF import. Suffix variants retain the full call in historic queries;
    // live candidates below derive the exact suffix token for selection.
    if (type == QStringLiteral("callarea") || type == QStringLiteral("px")
        || type == QStringLiteral("pxa") || type == QStringLiteral("pxplus")) {
        return QStringLiteral("upper(wpx)");
    }
    if (type == QStringLiteral("numsfx") || type == QStringLiteral("sfx")) {
        return QStringLiteral("upper(call)");
    }
    return {};
}

QString sqlQuotedList(const QStringList& values)
{
    QStringList quoted;
    for (QString value : values) {
        value = value.trimmed();
        if (!value.isEmpty()) {
            quoted.append(QStringLiteral("'%1'").arg(value.replace(QLatin1Char('\''), QStringLiteral("''"))));
        }
    }
    return quoted.join(QStringLiteral(","));
}

QString externalAwardScopeFilter(const ExternalAwardDefinition& definition)
{
    QString filter;
    QStringList bands;
    bool allBands = false;
    for (QString const& band : definition.bands) {
        if (band.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0
            || band.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) == 0
            || band.compare(QStringLiteral("Any"), Qt::CaseInsensitive) == 0) {
            allBands = true;
            break;
        }
        bands.append(band.toLower());
    }
    if (!allBands && !bands.isEmpty()) {
        filter += QStringLiteral(" AND lower(band) IN (%1)").arg(sqlQuotedList(bands));
    }

    bool digitalOnly = false;
    bool hasSpecificMode = false;
    QStringList modes;
    for (QString const& mode : definition.modes) {
        if (mode.compare(QStringLiteral("Digital"), Qt::CaseInsensitive) == 0) {
            digitalOnly = true;
        } else if (mode.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) != 0
                   && mode.compare(QStringLiteral("Any"), Qt::CaseInsensitive) != 0
                   && mode.compare(QStringLiteral("Phone"), Qt::CaseInsensitive) != 0) {
            hasSpecificMode = true;
            modes.append(mode.toUpper());
        }
    }
    if (hasSpecificMode) {
        filter += QStringLiteral(" AND upper(mode) IN (%1)").arg(sqlQuotedList(modes));
    } else if (digitalOnly) {
        filter += QStringLiteral(
            " AND upper(mode) IN ('FT8','FT4','FT2','JT4','JT9','JT65','Q65','MSK144','FST4','FST4W','WSPR')");
    }
    return filter;
}

bool externalAwardMatchesSpot(const ExternalAwardDefinition& definition,
                              const QString& band, const QString& mode)
{
    bool bandAllowed = definition.bands.isEmpty();
    for (QString const& candidate : definition.bands) {
        if (candidate.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0
            || candidate.compare(QStringLiteral("Any"), Qt::CaseInsensitive) == 0
            || candidate.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) == 0
            || candidate.compare(band, Qt::CaseInsensitive) == 0) {
            bandAllowed = true;
            break;
        }
    }
    if (!bandAllowed) return false;

    bool digitalOnly = false;
    bool modeAllowed = definition.modes.isEmpty();
    for (QString const& candidate : definition.modes) {
        if (candidate.compare(QStringLiteral("Any"), Qt::CaseInsensitive) == 0
            || candidate.compare(QStringLiteral("Mixed"), Qt::CaseInsensitive) == 0
            || candidate.compare(mode, Qt::CaseInsensitive) == 0) {
            modeAllowed = true;
            break;
        }
        digitalOnly = digitalOnly
            || candidate.compare(QStringLiteral("Digital"), Qt::CaseInsensitive) == 0;
    }
    if (!modeAllowed && digitalOnly) {
        static const QStringList digitalModes {
            QStringLiteral("FT8"), QStringLiteral("FT4"), QStringLiteral("FT2"),
            QStringLiteral("JT4"), QStringLiteral("JT9"), QStringLiteral("JT65"),
            QStringLiteral("Q65"), QStringLiteral("MSK144"), QStringLiteral("FST4"),
            QStringLiteral("FST4W"), QStringLiteral("WSPR")
        };
        modeAllowed = digitalModes.contains(mode.toUpper());
    }
    return modeAllowed;
}

QString awardAliasEntity(const ExternalAwardDefinition& definition,
                         QString candidate, bool prefixMatch)
{
    candidate = candidate.trimmed().toUpper();
    if (candidate.isEmpty()) return {};
    if (!definition.prefixWhitelist.isEmpty()) {
        bool accepted = false;
        for (QString const& prefix : definition.prefixWhitelist) {
            if (candidate.startsWith(prefix)) {
                accepted = true;
                break;
            }
        }
        if (!accepted) return {};
    }
    if (definition.aliases.isEmpty()) return candidate;
    for (QStringList const& group : definition.aliases) {
        for (QString const& alias : group) {
            if ((prefixMatch && candidate.startsWith(alias))
                || (!prefixMatch && candidate == alias)) {
                return group.constFirst();
            }
        }
    }
    return {};
}

QString externalAwardSpotEntity(const ExternalAwardDefinition& definition,
                                const QString& call, const QString& band,
                                const QString& grid,
                                const QString& dxcc, int cqZone,
                                const QString& state, const QString& continent,
                                const QString& county = {},
                                const QString& iota = {})
{
    QString const type = definition.type;
    if (type == QStringLiteral("grids")) return grid.left(4).toUpper();
    if (type == QStringLiteral("dxcc")) return dxcc.trimmed().toLower();
    if (type == QStringLiteral("dxcc2band")) {
        QString const entity = dxcc.trimmed().toLower();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("cqz")) return cqZone > 0 ? QString::number(cqZone) : QString();
    if (type == QStringLiteral("states")) {
        return state.trimmed().toUpper();
    }
    if (type == QStringLiteral("states2band")) {
        QString const entity = state.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("cnty")) return county.trimmed().toUpper();
    if (type == QStringLiteral("iota")) return iota.trimmed().toUpper();
    if (type == QStringLiteral("cont") || type == QStringLiteral("cont5")) {
        return continent.trimmed().toUpper();
    }
    if (type == QStringLiteral("cont2band") || type == QStringLiteral("cont52band")) {
        QString const entity = continent.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("call") || type == QStringLiteral("calls2dxcc")) {
        return call.trimmed().toUpper();
    }
    if (type == QStringLiteral("calls2band")) {
        QString const entity = call.trimmed().toUpper();
        return entity.isEmpty() ? QString() : entity + QStringLiteral("@") + band.toLower();
    }
    if (type == QStringLiteral("callarea") || type == QStringLiteral("px")
        || type == QStringLiteral("pxa") || type == QStringLiteral("pxplus")) {
        return awardAliasEntity(definition, wpxPrefix(call), true);
    }
    if (type == QStringLiteral("numsfx")) {
        int const lastDigit = call.lastIndexOf(QRegularExpression(QStringLiteral("[0-9]")));
        return lastDigit >= 0 && lastDigit + 1 < call.size()
            ? awardAliasEntity(definition, call.mid(lastDigit, 2), false) : QString();
    }
    if (type == QStringLiteral("sfx")) {
        int const lastDigit = call.lastIndexOf(QRegularExpression(QStringLiteral("[0-9]")));
        return lastDigit >= 0 && lastDigit + 1 < call.size()
            ? awardAliasEntity(definition, call.mid(lastDigit + 1), true) : QString();
    }
    return {};
}

qint64 spotAgeCutoff(const QString& filter, qint64 nowMs)
{
    QString const normalized = filter.trimmed().toLower();
    qint64 minutes = 0;
    if (normalized == QStringLiteral("5 min")) minutes = 5;
    else if (normalized == QStringLiteral("15 min")) minutes = 15;
    else if (normalized == QStringLiteral("1 hour")) minutes = 60;
    else if (normalized == QStringLiteral("6 hours")) minutes = 360;
    else if (normalized == QStringLiteral("24 hours")) minutes = 1440;
    else if (normalized == QStringLiteral("7 days")) minutes = 10080;
    return minutes > 0 ? nowMs - minutes * 60LL * 1000LL : 0;
}

QString activityTypeForMessage(const QString& message,
                               const QString& mode,
                               const QString& source,
                               bool isCq,
                               const QString& targetCall)
{
    QString const normalized = message.simplified().toUpper();
    if (source.compare(QStringLiteral("psk"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("PSK");
    }
    if (mode.compare(QStringLiteral("WSPR"), Qt::CaseInsensitive) == 0
        || mode.compare(QStringLiteral("FST4W"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("WSPR");
    }
    if (normalized == QStringLiteral("QRZ")
        || normalized.startsWith(QStringLiteral("QRZ "))) {
        return QStringLiteral("QRZ");
    }
    if (normalized.startsWith(QStringLiteral("CQ DX "))
        || normalized == QStringLiteral("CQ DX")) {
        return QStringLiteral("CQDX");
    }
    if (isCq) {
        return QStringLiteral("CQ");
    }
    if (!targetCall.trimmed().isEmpty()) {
        return QStringLiteral("QSX");
    }
    return QStringLiteral("LIVE");
}

qreal liveOpacityForAge(qint64 ageMs, int decayMinutes)
{
    qreal const lifetimeMs =
        static_cast<qreal>(qMax(1, decayMinutes)) * 60.0 * 1000.0;
    qreal const normalized =
        qBound<qreal>(0.0, static_cast<qreal>(ageMs) / lifetimeMs, 1.0);
    return 1.0 - 0.72 * normalized;
}

QString digestKey(const QStringList& parts)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        parts.join(QChar(0x1f)).toUtf8(), QCryptographicHash::Sha256).toHex());
}

QStringList sortedBands(QStringList values)
{
    values.removeDuplicates();
    values.removeAll(QString());
    std::sort(values.begin(), values.end(), [](QString const& left, QString const& right) {
        int const leftOrder = bandOrder(left);
        int const rightOrder = bandOrder(right);
        return leftOrder == rightOrder ? left < right : leftOrder < rightOrder;
    });
    values.prepend(QStringLiteral("All"));
    return values;
}

QStringList sortedModes(QStringList values)
{
    values.removeDuplicates();
    values.removeAll(QString());
    std::sort(values.begin(), values.end(), [](QString const& left, QString const& right) {
        return left.localeAwareCompare(right) < 0;
    });
    values.prepend(QStringLiteral("All"));
    return values;
}

class ScopedSqliteConnection
{
public:
    explicit ScopedSqliteConnection(const QString& path)
        : m_name(QStringLiteral("map_intelligence_%1")
                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
    {
        m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_name);
        m_database.setDatabaseName(path);
    }

    ~ScopedSqliteConnection()
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

bool execSql(QSqlDatabase& db, const QString& sql, QString* error)
{
    QSqlQuery query(db);
    if (query.exec(sql)) {
        return true;
    }
    if (error) {
        *error = query.lastError().text();
    }
    return false;
}

bool ensureColumn(QSqlDatabase& db,
                  const QString& table,
                  const QString& column,
                  const QString& definition,
                  QString* error)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        if (error) {
            *error = query.lastError().text();
        }
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return execSql(db,
                   QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                       .arg(table, column, definition),
                   error);
}

bool openMapDatabase(const QString& path,
                     std::unique_ptr<ScopedSqliteConnection>* connection,
                     QString* error)
{
    QFileInfo const info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Cannot create database directory: %1").arg(info.absolutePath());
        }
        return false;
    }

    auto candidate = std::make_unique<ScopedSqliteConnection>(path);
    QSqlDatabase& db = candidate->database();
    if (!db.open()) {
        if (error) {
            *error = db.lastError().text();
        }
        return false;
    }

    execSql(db, QStringLiteral("PRAGMA journal_mode=WAL"), nullptr);
    execSql(db, QStringLiteral("PRAGMA synchronous=NORMAL"), nullptr);
    execSql(db, QStringLiteral("PRAGMA busy_timeout=5000"), nullptr);
    execSql(db, QStringLiteral("PRAGMA temp_store=MEMORY"), nullptr);

    static const QStringList schema {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_meta ("
            " key TEXT PRIMARY KEY,"
            " value TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_qso ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " source_key TEXT NOT NULL UNIQUE,"
            " call TEXT,"
            " grid TEXT,"
            " grid4 TEXT,"
            " band TEXT,"
            " mode TEXT,"
            " qso_date TEXT,"
            " time_on TEXT,"
            " frequency_mhz REAL,"
            " confirmed INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid4 ON map_qso(grid4)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_band_mode ON map_qso(band, mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_confirmed ON map_qso(confirmed)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_spot ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " unique_key TEXT NOT NULL UNIQUE,"
            " call TEXT NOT NULL,"
            " grid TEXT,"
            " grid4 TEXT,"
            " band TEXT,"
            " mode TEXT,"
            " message TEXT,"
            " observed_utc TEXT NOT NULL,"
            " observed_ms INTEGER NOT NULL,"
            " frequency_hz INTEGER,"
            " snr INTEGER,"
            " source TEXT,"
            " hits INTEGER NOT NULL DEFAULT 1)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_observed ON map_spot(observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_band_mode ON map_spot(band, mode)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_grid4 ON map_spot(grid4)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_alert ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " alert_key TEXT NOT NULL UNIQUE,"
            " alert_type TEXT NOT NULL,"
            " call TEXT,"
            " grid TEXT,"
            " dxcc TEXT,"
            " message TEXT NOT NULL,"
            " created_ms INTEGER NOT NULL,"
            " is_read INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_alert_created ON map_alert(created_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_alert_unread ON map_alert(is_read, created_ms DESC)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_preference ("
            " call TEXT PRIMARY KEY COLLATE NOCASE,"
            " watched INTEGER NOT NULL DEFAULT 0,"
            " ignored INTEGER NOT NULL DEFAULT 0,"
            " updated_ms INTEGER NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_ignore ("
            " ignore_type TEXT NOT NULL COLLATE NOCASE,"
            " ignore_value TEXT NOT NULL COLLATE NOCASE,"
            " updated_ms INTEGER NOT NULL,"
            " PRIMARY KEY(ignore_type, ignore_value))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_roster_rule ("
            " rule_type TEXT NOT NULL COLLATE NOCASE,"
            " rule_value TEXT NOT NULL COLLATE NOCASE,"
            " rule_action TEXT NOT NULL,"
            " band TEXT NOT NULL DEFAULT '',"
            " mode TEXT NOT NULL DEFAULT '',"
            " enabled INTEGER NOT NULL DEFAULT 1,"
            " updated_ms INTEGER NOT NULL,"
            " PRIMARY KEY(rule_type, rule_value, band, mode))"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS map_spot_event ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " spot_key TEXT NOT NULL,"
            " call TEXT, grid TEXT, receiver_call TEXT, receiver_grid TEXT,"
            " band TEXT, mode TEXT, source TEXT, provider TEXT,"
            " observed_ms INTEGER NOT NULL, frequency_hz INTEGER, snr INTEGER,"
            " correlation INTEGER NOT NULL DEFAULT 0, activity_type TEXT)")
    };
    for (QString const& ddl : schema) {
        if (!execSql(db, ddl, error)) {
            return false;
        }
    }

    struct ColumnMigration {
        const char* table;
        const char* column;
        const char* definition;
    };
    static const ColumnMigration migrations[] {
        {"map_qso", "qso_epoch", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "source", "TEXT NOT NULL DEFAULT 'ADIF'"},
        {"map_qso", "grid6", "TEXT"},
        {"map_qso", "dxcc", "TEXT"},
        {"map_qso", "continent", "TEXT"},
        {"map_qso", "cq_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "itu_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "state", "TEXT"},
        {"map_qso", "county", "TEXT"},
        {"map_qso", "lotw_confirmed", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "eqsl_confirmed", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "oqrs", "INTEGER NOT NULL DEFAULT 0"},
        {"map_qso", "pota_ref", "TEXT"},
        {"map_qso", "iota", "TEXT"},
        {"map_qso", "wpx", "TEXT"},
        {"map_spot", "dxcc", "TEXT"},
        {"map_spot", "continent", "TEXT"},
        {"map_spot", "cq_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "itu_zone", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "state", "TEXT"},
        {"map_spot", "is_cq", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "target_call", "TEXT"},
        {"map_spot", "distance_km", "REAL NOT NULL DEFAULT -1"},
        {"map_spot", "grid6", "TEXT"},
        {"map_spot", "activity_type", "TEXT NOT NULL DEFAULT 'LIVE'"},
        {"map_spot", "receiver_call", "TEXT"},
        {"map_spot", "receiver_grid", "TEXT"},
        {"map_spot", "provider", "TEXT"},
        {"map_spot", "first_observed_ms", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "last_observed_ms", "INTEGER NOT NULL DEFAULT 0"},
        {"map_spot", "correlation_count", "INTEGER NOT NULL DEFAULT 0"}
    };
    for (ColumnMigration const& migration : migrations) {
        if (!ensureColumn(db,
                          QString::fromLatin1(migration.table),
                          QString::fromLatin1(migration.column),
                          QString::fromLatin1(migration.definition),
                          error)) {
            return false;
        }
    }
    static const QStringList extendedIndexes {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_period ON map_qso(qso_epoch DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_geo ON map_qso(continent, dxcc, cq_zone)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_source ON map_qso(source)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_call_status ON map_qso(call, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid_status ON map_qso(grid4, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_grid6_status ON map_qso(grid6, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_dxcc_status ON map_qso(dxcc, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_pota ON map_qso(pota_ref, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_iota ON map_qso(iota, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_wpx ON map_qso(wpx, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_qso_county ON map_qso(county, confirmed)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_geo ON map_spot(continent, dxcc, cq_zone)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_source_cq ON map_spot(source, is_cq)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_call_time ON map_spot(call, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_grid6 ON map_spot(grid6)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_activity_time ON map_spot(activity_type, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_receiver_time ON map_spot(receiver_call, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_correlation ON map_spot(correlation_count, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_time ON map_spot_event(observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_spot_event_grid ON map_spot_event(grid, observed_ms DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_roster_ignore_type ON map_roster_ignore(ignore_type, ignore_value)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_map_roster_rule_type ON map_roster_rule(rule_type, rule_value)")
    };
    for (QString const& ddl : extendedIndexes) {
        if (!execSql(db, ddl, error)) {
            return false;
        }
    }
    if (!execSql(db,
                 QStringLiteral(
                     "UPDATE map_qso SET grid6=upper(substr(grid,1,6))"
                     " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
                 error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot SET grid6=upper(substr(grid,1,6))"
                        " WHERE (grid6 IS NULL OR grid6='') AND length(grid)>=6"),
                    error)
        || !execSql(db,
                    QStringLiteral(
                        "UPDATE map_spot SET first_observed_ms=observed_ms, last_observed_ms=observed_ms"
                        " WHERE first_observed_ms=0 OR last_observed_ms=0"),
                    error)) {
        return false;
    }

    *connection = std::move(candidate);
    return true;
}

QString adifFingerprint(const QString& path)
{
    QFileInfo const info(path);
    return digestKey({
        QStringLiteral("map-adif-import-v%1").arg(kAdifImportFormatVersion),
        info.absoluteFilePath(),
        QString::number(info.size()),
        QString::number(info.lastModified().toMSecsSinceEpoch())
    });
}

QString callFromMessage(const QString& message)
{
    static const QRegularExpression callPattern(
        QStringLiteral(R"(\b(?:[A-Z0-9]{1,4}/)?[A-Z0-9]{1,3}[0-9][A-Z0-9]{1,4}(?:/[A-Z0-9]{1,4})?\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator matches = callPattern.globalMatch(message.toUpper());
    while (matches.hasNext()) {
        QString const token = matches.next().captured(0);
        if (!token.startsWith(QStringLiteral("RR"))
            && token != QStringLiteral("73")
            // A four-character Maidenhead locator such as JN70 also matches
            // the loose callsign expression.  It is never a station call.
            && normalizedGrid(token).isEmpty()) {
            return token;
        }
    }
    return {};
}

QString gridFromMessage(const QString& message)
{
    static const QRegularExpression gridPattern(
        QStringLiteral(R"(\b([A-R]{2}[0-9]{2}(?:[A-X]{2})?)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return normalizedGrid(gridPattern.match(message).captured(1));
}

bool messageAssociatesGridWithCall(const QString& message, const QString& call)
{
    QString const normalizedCall = call.trimmed().toUpper();
    QString const grid = gridFromMessage(message);
    if (normalizedCall.isEmpty() || grid.isEmpty()) {
        return false;
    }

    QStringList const tokens = message.toUpper().simplified().split(QLatin1Char(' '),
                                                                       Qt::SkipEmptyParts);
    int callIndex = -1;
    int gridIndex = -1;
    for (int index = 0; index < tokens.size(); ++index) {
        QString const token = tokens.at(index).trimmed();
        if (gridIndex < 0 && normalizedGrid(token) == grid) {
            gridIndex = index;
        }
        if (callIndex < 0 && token == normalizedCall) {
            callIndex = index;
        }
    }
    return callIndex >= 0 && gridIndex >= 0 && callIndex < gridIndex;
}

qint64 adifEpoch(const QString& date, const QString& time)
{
    QString const dateDigits = date.trimmed();
    if (!QRegularExpression(QStringLiteral("^\\d{8}$")).match(dateDigits).hasMatch()) {
        return 0;
    }

    QString digits = time.trimmed();
    while (digits.size() < 6) {
        digits.append(QLatin1Char('0'));
    }
    if (!QRegularExpression(QStringLiteral("^\\d{6}$")).match(digits.left(6)).hasMatch()) {
        return 0;
    }

    QDate const qsoDate = QDate::fromString(dateDigits, QStringLiteral("yyyyMMdd"));
    QTime const qsoTime = QTime::fromString(digits.left(6), QStringLiteral("hhmmss"));
    if (!qsoDate.isValid() || !qsoTime.isValid()) {
        return 0;
    }
    return QDateTime(qsoDate, qsoTime, QTimeZone::UTC).toMSecsSinceEpoch();
}

qint64 periodCutoffMs(const QString& period)
{
    qint64 const now = QDateTime::currentMSecsSinceEpoch();
    QString const value = period.trimmed().toLower();
    if (value == QStringLiteral("1 hour")) return now - 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("24 hours")) return now - 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("7 days")) return now - 7LL * 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("30 days")) return now - 30LL * 24LL * 60LL * 60LL * 1000LL;
    if (value == QStringLiteral("1 year")) return now - 365LL * 24LL * 60LL * 60LL * 1000LL;
    return 0;
}

QString normalizedFilter(const QString& value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("All") : value.trimmed();
}

QString rosterOrderColumn(const QString& sort)
{
    QString const value = sort.trimmed().toLower();
    if (value == QStringLiteral("call")) return QStringLiteral("s.call");
    if (value == QStringLiteral("snr")) return QStringLiteral("s.snr");
    if (value == QStringLiteral("distance")) return QStringLiteral("s.distance_km");
    if (value == QStringLiteral("dxcc")) return QStringLiteral("s.dxcc");
    if (value == QStringLiteral("grid")) return QStringLiteral("s.grid");
    return QStringLiteral("s.observed_ms");
}

bool isWasState(QString state)
{
    static const QSet<QString> states {
        QStringLiteral("AL"), QStringLiteral("AK"), QStringLiteral("AZ"),
        QStringLiteral("AR"), QStringLiteral("CA"), QStringLiteral("CO"),
        QStringLiteral("CT"), QStringLiteral("DE"), QStringLiteral("FL"),
        QStringLiteral("GA"), QStringLiteral("HI"), QStringLiteral("ID"),
        QStringLiteral("IL"), QStringLiteral("IN"), QStringLiteral("IA"),
        QStringLiteral("KS"), QStringLiteral("KY"), QStringLiteral("LA"),
        QStringLiteral("ME"), QStringLiteral("MD"), QStringLiteral("MA"),
        QStringLiteral("MI"), QStringLiteral("MN"), QStringLiteral("MS"),
        QStringLiteral("MO"), QStringLiteral("MT"), QStringLiteral("NE"),
        QStringLiteral("NV"), QStringLiteral("NH"), QStringLiteral("NJ"),
        QStringLiteral("NM"), QStringLiteral("NY"), QStringLiteral("NC"),
        QStringLiteral("ND"), QStringLiteral("OH"), QStringLiteral("OK"),
        QStringLiteral("OR"), QStringLiteral("PA"), QStringLiteral("RI"),
        QStringLiteral("SC"), QStringLiteral("SD"), QStringLiteral("TN"),
        QStringLiteral("TX"), QStringLiteral("UT"), QStringLiteral("VT"),
        QStringLiteral("VA"), QStringLiteral("WA"), QStringLiteral("WV"),
        QStringLiteral("WI"), QStringLiteral("WY")
    };
    return states.contains(state.trimmed().toUpper());
}

bool isLower48State(QString state)
{
    state = state.trimmed().toUpper();
    return isWasState(state)
        && state != QStringLiteral("AK")
        && state != QStringLiteral("HI");
}

} // namespace

MapIntelligenceService::MapIntelligenceService(QObject* parent,
                                               const QString& databasePath)
    : QObject(parent)
    , m_layerModel(new MapLayerModel(this))
    , m_pskFeedService(new MapPskFeedService(this))
    , m_databasePath(databasePath.trimmed().isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                .absoluteFilePath(QStringLiteral("map_intelligence.sqlite"))
          : QFileInfo(databasePath).absoluteFilePath())
{
    m_workerPool.setMaxThreadCount(1);
    m_workerPool.setExpiryTimeout(30000);
    m_workerPool.setThreadPriority(QThread::LowPriority);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("LiveMapLayers"));
    constexpr int kLayerPreferencesVersion = 2;
    int const savedLayerPreferencesVersion =
        settings.value(QStringLiteral("PreferencesVersion"), 0).toInt();
    // Older builds allowed the data-view selector to silently overwrite Live.
    // Restore the intended default once, then preserve every explicit choice.
    bool const migrateLiveDefault =
        savedLayerPreferencesVersion < kLayerPreferencesVersion;
    // Prior versions called this purely visual checkbox "OFFLINE BASE MAP".
    // It did not control network activity, therefore it must not silently
    // become the new operational Offline mode after an upgrade.
    bool const migrateOfflineMode = savedLayerPreferencesVersion < 2;
    bool const hasLivePreference = settings.contains(QStringLiteral("Live"));
    m_bandFilter = settings.value(QStringLiteral("Band"), QStringLiteral("All")).toString();
    m_modeFilter = settings.value(QStringLiteral("Mode"), QStringLiteral("All")).toString();
    m_periodFilter = settings.value(QStringLiteral("Period"), QStringLiteral("All time")).toString();
    m_continentFilter = settings.value(QStringLiteral("Continent"), QStringLiteral("All")).toString();
    m_dxccFilter = settings.value(QStringLiteral("Dxcc"), QStringLiteral("All")).toString();
    m_sourceFilter = settings.value(QStringLiteral("Source"), QStringLiteral("All")).toString();
    m_cqOnly = settings.value(QStringLiteral("CqOnly"), false).toBool();
    m_rosterSort = settings.value(QStringLiteral("RosterSort"), QStringLiteral("Time")).toString();
    m_rosterSortDescending =
        settings.value(QStringLiteral("RosterSortDescending"), true).toBool();
    m_rosterStatusFilter =
        settings.value(QStringLiteral("RosterStatus"), QStringLiteral("All")).toString();
    m_rosterHuntScope =
        settings.value(QStringLiteral("RosterHuntScope"), QStringLiteral("All time")).toString();
    m_rosterRetentionMinutes =
        qBound(1, settings.value(QStringLiteral("RosterRetentionMinutes"), 5).toInt(), 60);
    m_rosterCqOnly = settings.value(QStringLiteral("RosterCqOnly"), false).toBool();
    m_rosterTextFilter =
        settings.value(QStringLiteral("RosterTextFilter"), QString()).toString();
    m_rosterTextMode =
        settings.value(QStringLiteral("RosterTextMode"),
                       QStringLiteral("No filter")).toString();
    static const QStringList rosterTextModes {
        QStringLiteral("No filter"), QStringLiteral("Only"),
        QStringLiteral("Exclude"), QStringLiteral("Regex")
    };
    if (!rosterTextModes.contains(m_rosterTextMode, Qt::CaseInsensitive)) {
        m_rosterTextMode = QStringLiteral("No filter");
    }
    m_activeAwardProgram =
        settings.value(QStringLiteral("ActiveAwardProgram"), QStringLiteral("None")).toString();
    if (!availableAwardPrograms().contains(m_activeAwardProgram, Qt::CaseInsensitive)) {
        m_activeAwardProgram = QStringLiteral("None");
    }
    m_awardGoal =
        settings.value(QStringLiteral("AwardGoal"), QStringLiteral("Confirmed")).toString();
    if (!availableAwardGoals().contains(m_awardGoal, Qt::CaseInsensitive)) {
        m_awardGoal = QStringLiteral("Confirmed");
    }
    m_gridPrecision =
        settings.value(QStringLiteral("GridPrecision"), 4).toInt() == 6 ? 6 : 4;
    m_liveDecayMinutes =
        qBound(1, settings.value(QStringLiteral("LiveDecayMinutes"), 15).toInt(), 120);
    m_splitGridEnabled =
        settings.value(QStringLiteral("SplitGridEnabled"), true).toBool();
    m_coveragePushPinsEnabled =
        settings.value(QStringLiteral("CoveragePushPinsEnabled"), false).toBool();
    m_timeZoneOverlayEnabled =
        settings.value(QStringLiteral("TimeZoneOverlayEnabled"), false).toBool();
    m_pskDisplayMode =
        settings.value(QStringLiteral("PskDisplayMode"), QStringLiteral("Overlay")).toString();
    if (!availablePskDisplayModes().contains(m_pskDisplayMode, Qt::CaseInsensitive)) {
        m_pskDisplayMode = QStringLiteral("Overlay");
    }
    m_pskOpacityPercent =
        qBound(20, settings.value(QStringLiteral("PskOpacityPercent"), 65).toInt(), 100);
    m_spotAgeFilter = settings.value(QStringLiteral("SpotAgeFilter"),
                                     QStringLiteral("15 min")).toString();
    if (!availableSpotAgeFilters().contains(m_spotAgeFilter, Qt::CaseInsensitive)) {
        m_spotAgeFilter = QStringLiteral("15 min");
    }
    m_spotCorrelationFilter = settings.value(QStringLiteral("SpotCorrelationFilter"),
                                             QStringLiteral("All")).toString();
    if (!availableCorrelationFilters().contains(m_spotCorrelationFilter,
                                                Qt::CaseInsensitive)) {
        m_spotCorrelationFilter = QStringLiteral("All");
    }
    m_rosterVisibleColumns = settings.value(QStringLiteral("RosterVisibleColumns"),
                                             m_rosterVisibleColumns).toStringList();
    if (m_rosterVisibleColumns.isEmpty()) {
        m_rosterVisibleColumns = availableRosterColumns();
    }
    m_pskFeedService->setEndpoint(settings.value(QStringLiteral("PskMqttEndpoint"),
                                                  QStringLiteral("mqtt://mqtt.pskreporter.info:1883"))
                                      .toString());
    m_callLookupProvider =
        settings.value(QStringLiteral("CallLookupProvider"), QStringLiteral("QRZ")).toString();
    if (!availableCallLookupProviders().contains(m_callLookupProvider, Qt::CaseInsensitive)) {
        m_callLookupProvider = QStringLiteral("QRZ");
    }
    m_alertNewGridEnabled =
        settings.value(QStringLiteral("AlertNewGrid"), true).toBool();
    m_alertNewDxccEnabled =
        settings.value(QStringLiteral("AlertNewDxcc"), true).toBool();
    m_alertCqEnabled =
        settings.value(QStringLiteral("AlertCq"), true).toBool();
    m_alertCallPattern =
        settings.value(QStringLiteral("AlertCallPattern"), QString()).toString().trimmed().left(64);
    m_layerModel->setLayerEnabled(QStringLiteral("worked"),
                                  settings.value(QStringLiteral("Worked"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("confirmed"),
                                  settings.value(QStringLiteral("Confirmed"), true).toBool());
    m_layerModel->setLayerEnabled(
        QStringLiteral("live"),
        migrateLiveDefault || settings.value(QStringLiteral("Live"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("active"),
                                  settings.value(QStringLiteral("Active"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("missing"),
                                  settings.value(QStringLiteral("Missing"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("psk"),
                                  settings.value(QStringLiteral("Psk"), true).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("pota"),
                                  settings.value(QStringLiteral("Pota"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("states"),
                                  settings.value(QStringLiteral("States"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("counties"),
                                  settings.value(QStringLiteral("Counties"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("iota"),
                                  settings.value(QStringLiteral("Iota"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("wpx"),
                                  settings.value(QStringLiteral("Wpx"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("moon"),
                                  settings.value(QStringLiteral("Moon"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("propagation"),
                                  settings.value(QStringLiteral("Propagation"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("radar"),
                                  settings.value(QStringLiteral("Radar"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("lightning"),
                                  settings.value(QStringLiteral("Lightning"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("muf"),
                                  settings.value(QStringLiteral("Muf"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("fof2"),
                                  settings.value(QStringLiteral("Fof2"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("es"),
                                  settings.value(QStringLiteral("Es"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("aurora"),
                                  settings.value(QStringLiteral("Aurora"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("tropo"),
                                  settings.value(QStringLiteral("Tropo"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("earthquakes"),
                                  settings.value(QStringLiteral("Earthquakes"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("wildfires"),
                                  settings.value(QStringLiteral("Wildfires"), false).toBool());
    m_layerModel->setLayerEnabled(QStringLiteral("offline"),
                                  migrateOfflineMode
                                      ? false
                                      : settings.value(QStringLiteral("Offline"), false).toBool());

    if (migrateLiveDefault || !hasLivePreference) {
        settings.setValue(QStringLiteral("Live"), true);
    }
    if (migrateOfflineMode) {
        settings.setValue(QStringLiteral("Offline"), false);
    }
    settings.setValue(QStringLiteral("PreferencesVersion"),
                      kLayerPreferencesVersion);
    settings.sync();

    if (m_layerModel->layerEnabled(QStringLiteral("propagation"))) {
        static const QStringList propagationLayers {
            QStringLiteral("muf"), QStringLiteral("fof2"),
            QStringLiteral("es"), QStringLiteral("aurora")
        };
        for (QString const& layer : propagationLayers) {
            m_layerModel->setLayerEnabled(layer, true);
        }
    }

    connect(m_layerModel, &MapLayerModel::layerToggled, this,
            [this](QString const& id, bool enabled) {
        saveSetting(id.left(1).toUpper() + id.mid(1), enabled);
        if (id == QStringLiteral("worked")) {
            emit workedLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("confirmed")) {
            emit confirmedLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("live")) {
            emit liveLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("active")) {
            emit activeLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("missing")) {
            emit missingLayerEnabledChanged();
            rebuildVisibleCoverage();
        } else if (id == QStringLiteral("psk")) {
            emit pskLayerEnabledChanged();
            scheduleQuery();
        } else if (id == QStringLiteral("offline")) {
            setOfflineMode(enabled);
        } else if (id == QStringLiteral("propagation")) {
            static const QStringList propagationLayers {
                QStringLiteral("muf"), QStringLiteral("fof2"),
                QStringLiteral("es"), QStringLiteral("aurora")
            };
            for (QString const& layer : propagationLayers) {
                m_layerModel->setLayerEnabled(layer, enabled);
            }
        }
    });

    m_baseMapService = new MapBaseMapService(this);
    m_externalOverlayService =
        new MapExternalOverlayService(m_layerModel, this);
    {
        std::unique_ptr<ScopedSqliteConnection> connection;
        QString error;
        if (!openMapDatabase(m_databasePath, &connection, &error)) {
            qWarning().noquote()
                << "[MAPINT] database initialization failed:" << error;
        }
    }
    m_operationsService =
        new MapOperationsService(m_databasePath, m_layerModel, this);
    connect(m_pskFeedService, &MapPskFeedService::spotsReceived,
            this, &MapIntelligenceService::ingestPskSpots);
    connect(m_pskFeedService, &MapPskFeedService::enabledChanged, this, [this] {
        saveSetting(QStringLiteral("PskMqttEnabled"), m_pskFeedService->enabled());
    });
    connect(m_pskFeedService, &MapPskFeedService::endpointChanged, this, [this] {
        saveSetting(QStringLiteral("PskMqttEndpoint"), m_pskFeedService->endpoint());
    });
    m_pskFeedService->setEnabled(
        settings.value(QStringLiteral("PskMqttEnabled"), false).toBool());
    setOfflineMode(m_layerModel->layerEnabled(QStringLiteral("offline")));

    m_queryTimer = new QTimer(this);
    m_queryTimer->setSingleShot(true);
    m_queryTimer->setInterval(60);
    connect(m_queryTimer, &QTimer::timeout, this, [this] {
        queueSnapshotQuery(m_queryGeneration.load());
    });

    m_liveFlushTimer = new QTimer(this);
    m_liveFlushTimer->setSingleShot(true);
    m_liveFlushTimer->setInterval(250);
    connect(m_liveFlushTimer, &QTimer::timeout,
            this, &MapIntelligenceService::flushPendingLiveSpots);

    scheduleQuery();
}

MapIntelligenceService::~MapIntelligenceService()
{
    ++m_queryGeneration;
    ++m_importGeneration;
    ++m_gridDetailsGeneration;
    m_workerPool.clear();
    m_workerPool.waitForDone(5000);
}

QObject* MapIntelligenceService::layerModel() const
{
    return m_layerModel;
}

QObject* MapIntelligenceService::baseMapService() const
{
    return m_baseMapService;
}

QObject* MapIntelligenceService::externalOverlayService() const
{
    return m_externalOverlayService;
}

QObject* MapIntelligenceService::operationsService() const
{
    return m_operationsService;
}

QObject* MapIntelligenceService::pskFeedService() const
{
    return m_pskFeedService;
}

void MapIntelligenceService::setOfflineMode(bool offline)
{
    if (m_baseMapService) {
        m_baseMapService->setOfflineMode(offline);
    }
    if (m_externalOverlayService) {
        m_externalOverlayService->setOfflineMode(offline);
    }
    if (m_pskFeedService) {
        m_pskFeedService->setOfflineMode(offline);
    }
}

bool MapIntelligenceService::workedLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("worked"));
}

bool MapIntelligenceService::confirmedLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("confirmed"));
}

bool MapIntelligenceService::liveLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("live"));
}

QStringList MapIntelligenceService::availablePeriods() const
{
    return {
        QStringLiteral("All time"), QStringLiteral("1 hour"),
        QStringLiteral("24 hours"), QStringLiteral("7 days"),
        QStringLiteral("30 days"), QStringLiteral("1 year")
    };
}

QStringList MapIntelligenceService::availableRosterStatuses() const
{
    return {
        QStringLiteral("All"), QStringLiteral("New"),
        QStringLiteral("Unconfirmed"), QStringLiteral("Wanted"),
        QStringLiteral("Award"), QStringLiteral("Watched")
    };
}

QStringList MapIntelligenceService::availableRosterHuntScopes() const
{
    return {
        QStringLiteral("All time"), QStringLiteral("Band"),
        QStringLiteral("Band + mode")
    };
}

QStringList MapIntelligenceService::availableAwardPrograms() const
{
    QStringList programs {
        QStringLiteral("None"), QStringLiteral("DXCC"),
        QStringLiteral("Maidenhead"), QStringLiteral("WAZ"),
        QStringLiteral("WAS"), QStringLiteral("US48"),
        QStringLiteral("WAC"), QStringLiteral("ITU Zones"),
        QStringLiteral("POTA"), QStringLiteral("IOTA"),
        QStringLiteral("WPX")
    };
    for (ExternalAwardDefinition const& definition : externalAwardDefinitions()) {
        programs.append(definition.label);
    }
    return programs;
}

QStringList MapIntelligenceService::availableAwardGoals() const
{
    return {
        QStringLiteral("Worked"), QStringLiteral("Confirmed")
    };
}

QStringList MapIntelligenceService::availablePskDisplayModes() const
{
    return {QStringLiteral("Overlay"), QStringLiteral("Replace")};
}

QStringList MapIntelligenceService::availableSpotAgeFilters() const
{
    return {QStringLiteral("5 min"), QStringLiteral("15 min"),
            QStringLiteral("1 hour"), QStringLiteral("6 hours"),
            QStringLiteral("24 hours"), QStringLiteral("7 days"),
            QStringLiteral("All retained")};
}

QStringList MapIntelligenceService::availableCorrelationFilters() const
{
    return {QStringLiteral("All"), QStringLiteral("Correlated"),
            QStringLiteral("Local decode"), QStringLiteral("PSK Reporter"),
            QStringLiteral("OAMS")};
}

QStringList MapIntelligenceService::availableRosterColumns() const
{
    return {QStringLiteral("Grid"), QStringLiteral("Band"),
            QStringLiteral("Mode"), QStringLiteral("SNR"),
            QStringLiteral("DXCC"), QStringLiteral("Continent"),
            QStringLiteral("CQ zone"), QStringLiteral("ITU zone"),
            QStringLiteral("State"), QStringLiteral("County"),
            QStringLiteral("POTA"), QStringLiteral("IOTA"),
            QStringLiteral("WPX"), QStringLiteral("LoTW age"),
            QStringLiteral("eQSL age"), QStringLiteral("OQRS"),
            QStringLiteral("Age"), QStringLiteral("Source")};
}

QStringList MapIntelligenceService::availableCallLookupProviders() const
{
    return {
        QStringLiteral("QRZ"), QStringLiteral("HamQTH"),
        QStringLiteral("QRZCQ")
    };
}

bool MapIntelligenceService::activeLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("active"));
}

bool MapIntelligenceService::missingLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("missing"));
}

bool MapIntelligenceService::pskLayerEnabled() const
{
    return m_layerModel->layerEnabled(QStringLiteral("psk"));
}

bool MapIntelligenceService::liveEntryMatchesCurrentFilters(
    const QVariantMap& entry,
    qint64 dialFrequencyHz,
    const QString& band) const
{
    LiveSpot const spot = liveSpotFromEntry(entry, dialFrequencyHz, band);
    if (spot.message.isEmpty()) {
        return false;
    }

    auto matches = [](QString const& filter, QString const& value) {
        return filter.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0
            || filter.compare(value, Qt::CaseInsensitive) == 0;
    };

    if (!matches(m_bandFilter, spot.band)
        || !matches(m_modeFilter, spot.mode)
        || !matches(m_continentFilter, spot.continent)
        || !matches(m_dxccFilter, spot.dxcc)
        || !matches(m_sourceFilter, spot.source)) {
        return false;
    }

    qint64 const cutoff = periodCutoffMs(m_periodFilter);
    if (cutoff > 0 && spot.observedMs < cutoff) {
        return false;
    }
    return !m_cqOnly || spot.isCq;
}

void MapIntelligenceService::setBandFilter(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty() ? QStringLiteral("All") : value.trimmed();
    if (m_bandFilter.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_bandFilter = normalized;
    saveSetting(QStringLiteral("Band"), m_bandFilter);
    emit bandFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setModeFilter(const QString& value)
{
    QString const normalized = value.trimmed().isEmpty()
        ? QStringLiteral("All") : value.trimmed().toUpper();
    if (m_modeFilter.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_modeFilter = normalized;
    saveSetting(QStringLiteral("Mode"), m_modeFilter);
    emit modeFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setPeriodFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_periodFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_periodFilter = normalized;
    saveSetting(QStringLiteral("Period"), normalized);
    emit periodFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setContinentFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value).toUpper();
    if (m_continentFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_continentFilter = normalized;
    saveSetting(QStringLiteral("Continent"), normalized);
    emit continentFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setDxccFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_dxccFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_dxccFilter = normalized;
    saveSetting(QStringLiteral("Dxcc"), normalized);
    emit dxccFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSourceFilter(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_sourceFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_sourceFilter = normalized;
    saveSetting(QStringLiteral("Source"), normalized);
    emit sourceFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setCqOnly(bool enabled)
{
    if (m_cqOnly == enabled) return;
    m_cqOnly = enabled;
    saveSetting(QStringLiteral("CqOnly"), enabled);
    emit cqOnlyChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterSort(const QString& value)
{
    QString const normalized = normalizedFilter(value);
    if (m_rosterSort.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterSort = normalized;
    saveSetting(QStringLiteral("RosterSort"), normalized);
    emit rosterSortChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterSortDescending(bool descending)
{
    if (m_rosterSortDescending == descending) return;
    m_rosterSortDescending = descending;
    saveSetting(QStringLiteral("RosterSortDescending"), descending);
    emit rosterSortDescendingChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterStatusFilter(const QString& value)
{
    QString const normalized = availableRosterStatuses().contains(value, Qt::CaseInsensitive)
        ? availableRosterStatuses().at(
              availableRosterStatuses().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("All");
    if (m_rosterStatusFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterStatusFilter = normalized;
    saveSetting(QStringLiteral("RosterStatus"), normalized);
    emit rosterStatusFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterHuntScope(const QString& value)
{
    QString const normalized = availableRosterHuntScopes().contains(value, Qt::CaseInsensitive)
        ? availableRosterHuntScopes().at(
              availableRosterHuntScopes().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("All time");
    if (m_rosterHuntScope.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_rosterHuntScope = normalized;
    saveSetting(QStringLiteral("RosterHuntScope"), normalized);
    emit rosterHuntScopeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterRetentionMinutes(int minutes)
{
    int const bounded = qBound(1, minutes, 60);
    if (m_rosterRetentionMinutes == bounded) return;
    m_rosterRetentionMinutes = bounded;
    saveSetting(QStringLiteral("RosterRetentionMinutes"), bounded);
    emit rosterRetentionMinutesChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterCqOnly(bool enabled)
{
    if (m_rosterCqOnly == enabled) return;
    m_rosterCqOnly = enabled;
    saveSetting(QStringLiteral("RosterCqOnly"), enabled);
    emit rosterCqOnlyChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterTextFilter(const QString& value)
{
    QString const normalized = value.trimmed().left(128);
    if (m_rosterTextFilter == normalized) {
        return;
    }
    m_rosterTextFilter = normalized;
    saveSetting(QStringLiteral("RosterTextFilter"), normalized);
    emit rosterTextFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterTextMode(const QString& value)
{
    static const QStringList modes {
        QStringLiteral("No filter"), QStringLiteral("Only"),
        QStringLiteral("Exclude"), QStringLiteral("Regex")
    };
    int const index = modes.indexOf(value.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? modes.at(index) : QStringLiteral("No filter");
    if (m_rosterTextMode == normalized) {
        return;
    }
    m_rosterTextMode = normalized;
    saveSetting(QStringLiteral("RosterTextMode"), normalized);
    emit rosterTextModeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setActiveAwardProgram(const QString& value)
{
    QString const normalized = availableAwardPrograms().contains(value, Qt::CaseInsensitive)
        ? availableAwardPrograms().at(
              availableAwardPrograms().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("None");
    if (m_activeAwardProgram.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_activeAwardProgram = normalized;
    saveSetting(QStringLiteral("ActiveAwardProgram"), normalized);
    emit activeAwardProgramChanged();
    scheduleQuery();
}

void MapIntelligenceService::setAwardGoal(const QString& value)
{
    QString const normalized = availableAwardGoals().contains(value, Qt::CaseInsensitive)
        ? availableAwardGoals().at(
              availableAwardGoals().indexOf(value, 0, Qt::CaseInsensitive))
        : QStringLiteral("Confirmed");
    if (m_awardGoal.compare(normalized, Qt::CaseInsensitive) == 0) {
        return;
    }
    m_awardGoal = normalized;
    saveSetting(QStringLiteral("AwardGoal"), normalized);
    emit awardGoalChanged();
    scheduleQuery();
}

void MapIntelligenceService::setGridPrecision(int precision)
{
    int const normalized = precision == 6 ? 6 : 4;
    if (m_gridPrecision == normalized) {
        return;
    }
    m_gridPrecision = normalized;
    saveSetting(QStringLiteral("GridPrecision"), normalized);
    emit gridPrecisionChanged();
    scheduleQuery();
}

void MapIntelligenceService::setLiveDecayMinutes(int minutes)
{
    int const bounded = qBound(1, minutes, 120);
    if (m_liveDecayMinutes == bounded) {
        return;
    }
    m_liveDecayMinutes = bounded;
    saveSetting(QStringLiteral("LiveDecayMinutes"), bounded);
    emit liveDecayMinutesChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSplitGridEnabled(bool enabled)
{
    if (m_splitGridEnabled == enabled) {
        return;
    }
    m_splitGridEnabled = enabled;
    saveSetting(QStringLiteral("SplitGridEnabled"), enabled);
    emit splitGridEnabledChanged();
    scheduleQuery();
}

void MapIntelligenceService::setCoveragePushPinsEnabled(bool enabled)
{
    if (m_coveragePushPinsEnabled == enabled) {
        return;
    }
    m_coveragePushPinsEnabled = enabled;
    saveSetting(QStringLiteral("CoveragePushPinsEnabled"), enabled);
    emit coveragePushPinsEnabledChanged();
}

void MapIntelligenceService::setTimeZoneOverlayEnabled(bool enabled)
{
    if (m_timeZoneOverlayEnabled == enabled) {
        return;
    }
    m_timeZoneOverlayEnabled = enabled;
    saveSetting(QStringLiteral("TimeZoneOverlayEnabled"), enabled);
    emit timeZoneOverlayEnabledChanged();
}

void MapIntelligenceService::setPskDisplayMode(const QString& mode)
{
    int const index = availablePskDisplayModes().indexOf(
        mode.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0
        ? availablePskDisplayModes().at(index) : QStringLiteral("Overlay");
    if (m_pskDisplayMode == normalized) {
        return;
    }
    m_pskDisplayMode = normalized;
    saveSetting(QStringLiteral("PskDisplayMode"), normalized);
    emit pskDisplayModeChanged();
    scheduleQuery();
}

void MapIntelligenceService::setPskOpacityPercent(int percent)
{
    int const bounded = qBound(20, percent, 100);
    if (m_pskOpacityPercent == bounded) {
        return;
    }
    m_pskOpacityPercent = bounded;
    saveSetting(QStringLiteral("PskOpacityPercent"), bounded);
    emit pskOpacityPercentChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSpotAgeFilter(const QString& value)
{
    int const index = availableSpotAgeFilters().indexOf(value.trimmed(), 0,
                                                         Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? availableSpotAgeFilters().at(index)
                                          : QStringLiteral("15 min");
    if (m_spotAgeFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_spotAgeFilter = normalized;
    saveSetting(QStringLiteral("SpotAgeFilter"), normalized);
    emit spotAgeFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setSpotCorrelationFilter(const QString& value)
{
    int const index = availableCorrelationFilters().indexOf(value.trimmed(), 0,
                                                            Qt::CaseInsensitive);
    QString const normalized = index >= 0 ? availableCorrelationFilters().at(index)
                                          : QStringLiteral("All");
    if (m_spotCorrelationFilter.compare(normalized, Qt::CaseInsensitive) == 0) return;
    m_spotCorrelationFilter = normalized;
    saveSetting(QStringLiteral("SpotCorrelationFilter"), normalized);
    emit spotCorrelationFilterChanged();
    scheduleQuery();
}

void MapIntelligenceService::setRosterVisibleColumns(const QStringList& columns)
{
    QStringList normalized;
    QStringList const known = availableRosterColumns();
    for (QString const& column : columns) {
        int const index = known.indexOf(column.trimmed(), 0, Qt::CaseInsensitive);
        if (index >= 0 && !normalized.contains(known.at(index))) {
            normalized.append(known.at(index));
        }
    }
    if (normalized.isEmpty()) {
        normalized = {QStringLiteral("Grid"), QStringLiteral("Band"),
                      QStringLiteral("Mode"), QStringLiteral("SNR"),
                      QStringLiteral("DXCC"), QStringLiteral("Age")};
    }
    if (m_rosterVisibleColumns == normalized) return;
    m_rosterVisibleColumns = normalized;
    saveSetting(QStringLiteral("RosterVisibleColumns"), normalized);
    emit rosterVisibleColumnsChanged();
}

void MapIntelligenceService::setCallLookupProvider(const QString& provider)
{
    int const index = availableCallLookupProviders().indexOf(
        provider.trimmed(), 0, Qt::CaseInsensitive);
    QString const normalized = index >= 0
        ? availableCallLookupProviders().at(index) : QStringLiteral("QRZ");
    if (m_callLookupProvider == normalized) {
        return;
    }
    m_callLookupProvider = normalized;
    saveSetting(QStringLiteral("CallLookupProvider"), normalized);
    emit callLookupProviderChanged();
}

void MapIntelligenceService::setAlertNewGridEnabled(bool enabled)
{
    if (m_alertNewGridEnabled == enabled) return;
    m_alertNewGridEnabled = enabled;
    saveSetting(QStringLiteral("AlertNewGrid"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertNewDxccEnabled(bool enabled)
{
    if (m_alertNewDxccEnabled == enabled) return;
    m_alertNewDxccEnabled = enabled;
    saveSetting(QStringLiteral("AlertNewDxcc"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertCqEnabled(bool enabled)
{
    if (m_alertCqEnabled == enabled) return;
    m_alertCqEnabled = enabled;
    saveSetting(QStringLiteral("AlertCq"), enabled);
    emit alertRulesChanged();
}

void MapIntelligenceService::setAlertCallPattern(const QString& pattern)
{
    QString const normalized = pattern.trimmed().left(64);
    if (m_alertCallPattern == normalized) return;
    m_alertCallPattern = normalized;
    saveSetting(QStringLiteral("AlertCallPattern"), normalized);
    emit alertRulesChanged();
}

void MapIntelligenceService::setWorkedLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("worked"), enabled);
}

void MapIntelligenceService::setConfirmedLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("confirmed"), enabled);
}

void MapIntelligenceService::setLiveLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("live"), enabled);
}

void MapIntelligenceService::setActiveLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("active"), enabled);
}

void MapIntelligenceService::setMissingLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("missing"), enabled);
}

void MapIntelligenceService::setPskLayerEnabled(bool enabled)
{
    m_layerModel->setLayerEnabled(QStringLiteral("psk"), enabled);
}

void MapIntelligenceService::reloadFromAdif(const QString& path)
{
    QString const cleanPath = QFileInfo(path).absoluteFilePath();
    quint64 const importGeneration = ++m_importGeneration;
    quint64 const queryGeneration = ++m_queryGeneration;
    if (m_sourcePath != cleanPath) {
        m_sourcePath = cleanPath;
        emit sourcePathChanged();
    }
    setLoading(true);

    QString const database = m_databasePath;
    QueryOptions const options {
        m_bandFilter, m_modeFilter, m_periodFilter, m_continentFilter,
        m_dxccFilter, m_sourceFilter, m_rosterSort, m_rosterStatusFilter,
        m_rosterHuntScope, m_activeAwardProgram, m_awardGoal,
        m_rosterRetentionMinutes, m_gridPrecision, m_liveDecayMinutes,
        m_cqOnly, m_rosterSortDescending, m_rosterCqOnly,
        m_splitGridEnabled, m_rosterTextFilter, m_rosterTextMode,
        pskLayerEnabled(), m_pskDisplayMode, m_pskOpacityPercent / 100.0,
        m_spotAgeFilter, m_spotCorrelationFilter
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, cleanPath, database, options, importGeneration, queryGeneration] {
            QByteArray data;
            QString error;
            QFile file(cleanPath);
            bool const sourceReadable = file.open(QIODevice::ReadOnly);
            if (sourceReadable) {
                data = file.read(kMaxAdifBytes + 1);
                if (data.size() > kMaxAdifBytes) {
                    data.truncate(kMaxAdifBytes);
                }
            } else {
                error = QStringLiteral("Cannot read ADIF log %1: %2")
                            .arg(cleanPath, file.errorString());
            }

            // Do not replace a valid local map cache with an empty import when
            // a log is temporarily unavailable (network drive, permissions or
            // a log rotation in progress).
            bool const imported = sourceReadable && importAdifIntoDatabase(
                database, cleanPath, data, adifFingerprint(cleanPath), &error);
            Snapshot snapshot = queryDatabase(database, options);
            if ((!sourceReadable || !imported) && snapshot.error.isEmpty()) {
                snapshot.error = error;
            }

            if (!guard) {
                return;
            }
            QMetaObject::invokeMethod(guard.data(),
                [guard, importGeneration, queryGeneration, snapshot = std::move(snapshot)]() mutable {
                    if (!guard) {
                        return;
                    }
                    if (importGeneration == guard->m_importGeneration.load()) {
                        guard->setLoading(false);
                    }
                    guard->applySnapshot(queryGeneration, std::move(snapshot));
                    if (guard->m_operationsService) {
                        guard->m_operationsService->refreshLogbook();
                    }
                }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::appendAdifRecord(const QByteArray& record)
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, record] {
        QList<QsoRecord> const records = parseAdif(record);
        QString error;
        if (!records.isEmpty()) {
            appendQsoRecords(database, records, &error);
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] incremental ADIF insert failed:" << error;
            }
            if (guard->m_operationsService) {
                guard->m_operationsService->refreshLogbook();
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::refresh()
{
    scheduleQuery();
    if (!m_selectedGrid.isEmpty()) {
        selectGrid(m_selectedGrid);
    }
}

void MapIntelligenceService::selectGrid(const QString& grid)
{
    QString const fullGrid = normalizedGrid(grid);
    QString const normalized =
        fullGrid.left(m_gridPrecision == 6 && fullGrid.size() >= 6 ? 6 : 4);
    if (normalized.isEmpty()) {
        clearGridSelection();
        return;
    }

    quint64 const generation = ++m_gridDetailsGeneration;
    m_selectedGrid = normalized;
    m_selectedGridLive.clear();
    m_selectedGridQsos.clear();
    m_selectedGridSummary = {
        {QStringLiteral("grid"), normalized},
        {QStringLiteral("workedCount"), 0},
        {QStringLiteral("confirmedCount"), 0},
        {QStringLiteral("activeCount"), 0},
        {QStringLiteral("pskCount"), 0}
    };
    for (QVariant const& value : std::as_const(m_rawCoverage)) {
        QVariantMap const row = value.toMap();
        if (row.value(QStringLiteral("grid")).toString()
                .compare(normalized, Qt::CaseInsensitive) == 0) {
            m_selectedGridSummary = row;
            break;
        }
    }
    emit gridDetailsChanged();
    setGridDetailsLoading(true);

    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalized, generation] {
        GridDetails details = queryGridDetails(database, normalized);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(),
            [guard, generation, normalized, details = std::move(details)]() mutable {
                if (guard) {
                    guard->applyGridDetails(generation, normalized,
                                            std::move(details));
                }
            }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::clearGridSelection()
{
    ++m_gridDetailsGeneration;
    bool const hadSelection = !m_selectedGrid.isEmpty()
        || !m_selectedGridSummary.isEmpty()
        || !m_selectedGridLive.isEmpty()
        || !m_selectedGridQsos.isEmpty();
    m_selectedGrid.clear();
    m_selectedGridSummary.clear();
    m_selectedGridLive.clear();
    m_selectedGridQsos.clear();
    setGridDetailsLoading(false);
    if (hadSelection) {
        emit gridDetailsChanged();
    }
}

void MapIntelligenceService::clearLiveSpots()
{
    m_pendingDecodes.clear();
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearLiveSpotRows(database, &error);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear live spots failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::clearAlerts()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearAlertRows(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear alerts failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::markAlertsRead()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        markAlertRowsRead(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] mark alerts read failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterCallWatched(const QString& call, bool watched)
{
    QString const normalizedCall = call.trimmed().toUpper();
    if (normalizedCall.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedCall, watched] {
        QString error;
        updateRosterPreference(database, normalizedCall, watched, false, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update watched call failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterCallIgnored(const QString& call, bool ignored)
{
    QString const normalizedCall = call.trimmed().toUpper();
    if (normalizedCall.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedCall, ignored] {
        QString error;
        updateRosterPreference(database, normalizedCall, false, ignored, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update ignored call failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::setRosterDxccIgnored(const QString& dxcc, bool ignored)
{
    QString const normalizedDxcc = dxcc.trimmed();
    if (normalizedDxcc.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, database, normalizedDxcc, ignored] {
            QString error;
            updateRosterIgnore(database, QStringLiteral("DXCC"),
                               normalizedDxcc, ignored, &error);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, error] {
                if (!guard) return;
                if (!error.isEmpty()) {
                    qWarning().noquote()
                        << "[MAPINT] update ignored DXCC failed:" << error;
                }
                guard->scheduleQuery();
            }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::removeRosterPreference(const QString& type,
                                                    const QString& value)
{
    QString const normalizedType = type.trimmed().toUpper();
    QString const normalizedValue = value.trimmed();
    if (normalizedValue.isEmpty()
        || (normalizedType != QStringLiteral("WATCH")
            && normalizedType != QStringLiteral("CALL")
            && normalizedType != QStringLiteral("DXCC"))) {
        return;
    }
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create(
        [guard, database, normalizedType, normalizedValue] {
            QString error;
            removeRosterPreferenceRow(database, normalizedType,
                                      normalizedValue, &error);
            if (!guard) return;
            QMetaObject::invokeMethod(guard.data(), [guard, error] {
                if (!guard) return;
                if (!error.isEmpty()) {
                    qWarning().noquote()
                        << "[MAPINT] remove roster preference failed:" << error;
                }
                guard->scheduleQuery();
            }, Qt::QueuedConnection);
        }));
}

void MapIntelligenceService::clearRosterPreferences()
{
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database] {
        QString error;
        clearRosterPreferenceRows(database, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] clear roster preferences failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::configurePskFeed(const QString& callsign,
                                              const QString& grid)
{
    if (!m_pskFeedService) return;
    m_pskFeedService->configureStation(callsign, grid);
    if (!m_pskFeedService->enabled()) {
        m_pskFeedService->setEnabled(true);
        saveSetting(QStringLiteral("PskMqttEnabled"), true);
    }
}

void MapIntelligenceService::setRosterRule(const QString& type,
                                           const QString& value,
                                           const QString& action,
                                           const QString& band,
                                           const QString& mode)
{
    QString const normalizedType = type.trimmed().toUpper().left(24);
    QString const normalizedValue = value.trimmed().toUpper().left(128);
    QString const normalizedAction = action.trimmed().toUpper().left(24);
    if (normalizedType.isEmpty() || normalizedValue.isEmpty()
        || !QStringList {QStringLiteral("WANTED"), QStringLiteral("IGNORE"),
                         QStringLiteral("WATCH")}
                .contains(normalizedAction)) {
        return;
    }
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedType,
                                         normalizedValue, normalizedAction,
                                         band, mode] {
        QString error;
        updateRosterRuleRow(database, normalizedType, normalizedValue,
                            normalizedAction, band, mode, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] update roster rule failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::removeRosterRule(const QString& type,
                                              const QString& value,
                                              const QString& band,
                                              const QString& mode)
{
    QString const normalizedType = type.trimmed().toUpper().left(24);
    QString const normalizedValue = value.trimmed().toUpper().left(128);
    if (normalizedType.isEmpty() || normalizedValue.isEmpty()) return;
    QString const database = m_databasePath;
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, normalizedType,
                                         normalizedValue, band, mode] {
        QString error;
        removeRosterRuleRow(database, normalizedType, normalizedValue,
                            band, mode, &error);
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] remove roster rule failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::ingestPskSpots(const QVariantList& rows,
                                            const QString& senderCall,
                                            const QString& senderGrid)
{
    queuePskSpots(rows, senderCall, senderGrid, false);
}

void MapIntelligenceService::replacePskHeardBySpots(const QVariantList& rows,
                                                    const QString& senderCall,
                                                    const QString& senderGrid)
{
    queuePskSpots(rows, senderCall, senderGrid, true);
}

void MapIntelligenceService::queuePskSpots(const QVariantList& rows,
                                           const QString& senderCall,
                                           const QString& senderGrid,
                                           bool replaceHeardBySnapshot)
{
    if (rows.isEmpty() && !replaceHeardBySnapshot) return;
    QString const database = m_databasePath;
    AlertRules const rules {
        m_alertNewGridEnabled, m_alertNewDxccEnabled,
        m_alertCqEnabled, m_alertCallPattern
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, rows, senderCall, senderGrid,
                                          rules, replaceHeardBySnapshot] {
        QList<LiveSpot> spots;
        spots.reserve(rows.size());
        qint64 const now = QDateTime::currentMSecsSinceEpoch();
        for (QVariant const& value : rows) {
            QVariantMap const row = value.toMap();
            LiveSpot spot;
            spot.call = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
            spot.grid = normalizedGrid(row.value(QStringLiteral("grid")).toString());
            spot.grid4 = spot.grid.left(4);
            spot.grid6 = spot.grid.size() >= 6 ? spot.grid.left(6) : QString();
            spot.frequencyHz = row.value(QStringLiteral("freq"),
                                         row.value(QStringLiteral("frequency"))).toLongLong();
            spot.band = normalizedBand(row.value(QStringLiteral("band")).toString(),
                                       spot.frequencyHz / 1.0e6);
            spot.mode = normalizedMode(row.value(QStringLiteral("mode")).toString());
            spot.snr = row.value(QStringLiteral("snr")).toInt();
            spot.distanceKm = row.value(QStringLiteral("distKm"), -1.0).toDouble();
            spot.dxcc = row.value(QStringLiteral("dxcc")).toString().trimmed();
            spot.continent = row.value(QStringLiteral("continent")).toString().trimmed().toUpper();
            spot.cqZone = row.value(QStringLiteral("cqZone")).toInt();
            spot.ituZone = row.value(QStringLiteral("ituZone")).toInt();
            spot.state = row.value(QStringLiteral("state")).toString().trimmed().toUpper();
            spot.source = row.value(QStringLiteral("source"),
                                    QStringLiteral("psk")).toString().trimmed().toLower();
            if (spot.source.isEmpty()) spot.source = QStringLiteral("psk");
            spot.targetCall = senderCall.trimmed().toUpper();
            spot.receiverCall = row.value(QStringLiteral("receiverCall"), senderCall)
                                    .toString().trimmed().toUpper();
            spot.receiverGrid = normalizedGrid(
                row.value(QStringLiteral("receiverGrid"), senderGrid).toString());
            spot.provider = row.value(QStringLiteral("provider"),
                                      QStringLiteral("PSK Reporter")).toString().trimmed();
            spot.message = QStringLiteral("%1 heard %2 from %3")
                               .arg(spot.call, senderCall.trimmed().toUpper(),
                                    senderGrid.trimmed().toUpper());
            spot.observedMs = row.value(QStringLiteral("timestamp")).toLongLong();
            if (spot.observedMs > 0 && spot.observedMs < 100000000000LL) {
                spot.observedMs *= 1000;
            }
            if (spot.observedMs <= 0 || spot.observedMs > now + 5 * 60 * 1000LL) {
                spot.observedMs = now;
            }
            spot.observedUtc = QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC)
                                   .toString(Qt::ISODate);
            spot.observedUtc = QDateTime::fromMSecsSinceEpoch(spot.observedMs, QTimeZone::UTC)
                                   .toString(Qt::ISODate);
            spot.activityType = spot.source.compare(QStringLiteral("oams"), Qt::CaseInsensitive) == 0
                ? QStringLiteral("OAMS") : QStringLiteral("PSK");
            spot.uniqueKey = digestKey({
                spot.source, spot.call, spot.grid, spot.receiverCall,
                QString::number(spot.frequencyHz), QString::number(spot.observedMs / 60000)
            });
            if (!spot.call.isEmpty()) spots.append(std::move(spot));
        }
        QString error;
        if (replaceHeardBySnapshot) {
            clearPskHeardByRows(database, &error);
        }
        if (error.isEmpty() && !spots.isEmpty()) {
            appendLiveSpots(database, spots, rules, &error);
        }
        if (!guard) return;
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) return;
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] PSK spot batch failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::ingestDecodeEntry(const QVariantMap& entry,
                                               qint64 dialFrequencyHz,
                                               const QString& band)
{
    if (entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()
        || entry.value(QStringLiteral("message")).toString().trimmed().isEmpty()) {
        return;
    }
    if (m_pendingDecodes.size() >= kMaxPendingLiveSpots) {
        m_pendingDecodes.removeFirst();
    }
    m_pendingDecodes.append(PendingDecode {entry, dialFrequencyHz, band});
    if (!m_liveFlushTimer->isActive()) {
        m_liveFlushTimer->start();
    }
}

void MapIntelligenceService::scheduleQuery()
{
    ++m_queryGeneration;
    m_queryTimer->start();
}

void MapIntelligenceService::flushPendingLiveSpots()
{
    if (m_pendingDecodes.isEmpty()) {
        return;
    }
    QList<PendingDecode> pending;
    pending.swap(m_pendingDecodes);
    QString const database = m_databasePath;
    AlertRules const rules {
        m_alertNewGridEnabled, m_alertNewDxccEnabled,
        m_alertCqEnabled, m_alertCallPattern
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, rules,
                                         pending = std::move(pending)] {
        QList<LiveSpot> spots;
        spots.reserve(pending.size());
        for (PendingDecode const& decode : pending) {
            LiveSpot spot = liveSpotFromEntry(
                decode.entry, decode.dialFrequencyHz, decode.band);
            if (!spot.call.isEmpty()) {
                spots.append(std::move(spot));
            }
        }
        QString error;
        if (!spots.isEmpty()) {
            appendLiveSpots(database, spots, rules, &error);
        }
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(), [guard, error] {
            if (!guard) {
                return;
            }
            if (!error.isEmpty()) {
                qWarning().noquote() << "[MAPINT] live spot batch failed:" << error;
            }
            guard->scheduleQuery();
        }, Qt::QueuedConnection);
    }));
}

QList<MapIntelligenceService::QsoRecord>
MapIntelligenceService::parseAdif(const QByteArray& data)
{
    QList<QsoRecord> records;
    std::shared_ptr<const DxccLookup> const dxccLookup = adifDxccLookup();
    auto enrichRecord = [dxccLookup](QsoRecord& record) {
        if (!dxccLookup || record.call.isEmpty()) {
            return;
        }

        DxccEntity const entity = dxccLookup->lookup(record.call);
        if (!entity.isValid()) {
            return;
        }
        if (record.dxcc.isEmpty()) {
            record.dxcc = entity.name;
        }
        if (record.continent.isEmpty()) {
            record.continent = entity.continent;
        }
        if (record.cqZone <= 0) {
            record.cqZone = entity.cqZone;
        }
        if (record.ituZone <= 0) {
            record.ituZone = entity.ituZone;
        }
    };
    QHash<QString, QString> fields;
    int position = 0;
    while (position < data.size() && records.size() < kMaxAdifRecords) {
        int const open = data.indexOf('<', position);
        if (open < 0) {
            break;
        }
        int const close = data.indexOf('>', open + 1);
        if (close < 0) {
            break;
        }

        QList<QByteArray> const parts = data.mid(open + 1, close - open - 1).trimmed().split(':');
        QString const name = QString::fromLatin1(parts.value(0)).trimmed().toUpper();
        position = close + 1;

        auto appendRecord = [&records, &enrichRecord](QHash<QString, QString> const& recordFields) {
            if (recordFields.isEmpty()) {
                return;
            }
            QsoRecord record;
            record.call = recordFields.value(QStringLiteral("CALL")).trimmed().toUpper();
            record.grid = normalizedGrid(recordFields.value(QStringLiteral("GRIDSQUARE")));
            record.grid4 = record.grid.left(4);
            record.grid6 = record.grid.size() >= 6 ? record.grid.left(6) : QString();
            bool frequencyOk = false;
            record.frequencyMhz = recordFields.value(QStringLiteral("FREQ")).toDouble(&frequencyOk);
            if (!frequencyOk) {
                record.frequencyMhz = 0.0;
            }
            record.band = normalizedBand(recordFields.value(QStringLiteral("BAND")),
                                         record.frequencyMhz);
            record.mode = normalizedMode(recordFields.value(QStringLiteral("MODE")),
                                         recordFields.value(QStringLiteral("SUBMODE")));
            record.qsoDate = recordFields.value(QStringLiteral("QSO_DATE")).trimmed();
            record.timeOn = recordFields.value(QStringLiteral("TIME_ON")).trimmed();
            record.qsoEpoch = adifEpoch(record.qsoDate, record.timeOn);
            record.source = normalizedFilter(
                recordFields.value(QStringLiteral("APP_DECODIUM_SOURCE")));
            if (record.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0) {
                record.source = QStringLiteral("ADIF");
            }
            record.dxcc = recordFields.value(QStringLiteral("COUNTRY")).trimmed();
            record.continent = recordFields.value(QStringLiteral("CONT")).trimmed().toUpper();
            record.cqZone = recordFields.value(QStringLiteral("CQZ")).toInt();
            record.ituZone = recordFields.value(QStringLiteral("ITUZ")).toInt();
            record.state = recordFields.value(QStringLiteral("STATE")).trimmed().toUpper();
            record.county = recordFields.value(QStringLiteral("CNTY")).trimmed().toUpper();
            if (record.county.isEmpty()) {
                record.county = recordFields.value(QStringLiteral("COUNTY")).trimmed().toUpper();
            }
            record.lotwConfirmed = adifYes(recordFields.value(QStringLiteral("LOTW_QSL_RCVD")));
            record.eqslConfirmed = adifYes(recordFields.value(QStringLiteral("EQSL_QSL_RCVD")));
            record.oqrs = adifYes(recordFields.value(QStringLiteral("OQRS")));
            record.potaReference =
                normalizedPota(recordFields.value(QStringLiteral("POTA_REF")));
            if (record.potaReference.isEmpty()
                && recordFields.value(QStringLiteral("SIG"))
                       .compare(QStringLiteral("POTA"), Qt::CaseInsensitive) == 0) {
                record.potaReference =
                    normalizedPota(recordFields.value(QStringLiteral("SIG_INFO")));
            }
            record.iotaReference =
                normalizedIota(recordFields.value(QStringLiteral("IOTA")));
            record.wpxPrefix = wpxPrefix(record.call);
            record.confirmed = isConfirmed(recordFields);
            enrichRecord(record);
            record.sourceKey = digestKey({
                record.call, record.grid, record.band, record.mode,
                record.qsoDate, record.timeOn,
                QString::number(record.frequencyMhz, 'f', 6)
            });
            records.append(std::move(record));
        };

        if (name == QStringLiteral("EOH")) {
            fields.clear();
            continue;
        }
        if (name == QStringLiteral("EOR")) {
            appendRecord(fields);
            fields.clear();
            continue;
        }
        if (parts.size() < 2) {
            continue;
        }

        bool lengthOk = false;
        int const length = parts.at(1).trimmed().toInt(&lengthOk);
        if (!lengthOk || length < 0 || position + length > data.size()) {
            continue;
        }
        static const QSet<QString> wanted {
            QStringLiteral("CALL"), QStringLiteral("GRIDSQUARE"),
            QStringLiteral("BAND"), QStringLiteral("FREQ"),
            QStringLiteral("MODE"), QStringLiteral("SUBMODE"),
            QStringLiteral("QSO_DATE"), QStringLiteral("TIME_ON"),
            QStringLiteral("QSL_RCVD"), QStringLiteral("LOTW_QSL_RCVD"),
            QStringLiteral("EQSL_QSL_RCVD"), QStringLiteral("COUNTRY"),
            QStringLiteral("CONT"), QStringLiteral("CQZ"),
            QStringLiteral("ITUZ"), QStringLiteral("STATE"),
            QStringLiteral("CNTY"), QStringLiteral("COUNTY"),
            QStringLiteral("OQRS"),
            QStringLiteral("POTA_REF"), QStringLiteral("IOTA"),
            QStringLiteral("SIG"), QStringLiteral("SIG_INFO"),
            QStringLiteral("APP_DECODIUM_SOURCE")
        };
        if (wanted.contains(name)) {
            fields.insert(name, decodedAdifValue(data.mid(position, length)));
        }
        position += length;
    }
    if (!fields.isEmpty() && records.size() < kMaxAdifRecords) {
        QsoRecord record;
        record.call = fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        record.grid = normalizedGrid(fields.value(QStringLiteral("GRIDSQUARE")));
        record.grid4 = record.grid.left(4);
        record.grid6 = record.grid.size() >= 6 ? record.grid.left(6) : QString();
        bool ok = false;
        record.frequencyMhz = fields.value(QStringLiteral("FREQ")).toDouble(&ok);
        if (!ok) record.frequencyMhz = 0.0;
        record.band = normalizedBand(fields.value(QStringLiteral("BAND")), record.frequencyMhz);
        record.mode = normalizedMode(fields.value(QStringLiteral("MODE")),
                                     fields.value(QStringLiteral("SUBMODE")));
        record.qsoDate = fields.value(QStringLiteral("QSO_DATE")).trimmed();
        record.timeOn = fields.value(QStringLiteral("TIME_ON")).trimmed();
        record.qsoEpoch = adifEpoch(record.qsoDate, record.timeOn);
        record.source = normalizedFilter(fields.value(QStringLiteral("APP_DECODIUM_SOURCE")));
        if (record.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0) {
            record.source = QStringLiteral("ADIF");
        }
        record.dxcc = fields.value(QStringLiteral("COUNTRY")).trimmed();
        record.continent = fields.value(QStringLiteral("CONT")).trimmed().toUpper();
        record.cqZone = fields.value(QStringLiteral("CQZ")).toInt();
        record.ituZone = fields.value(QStringLiteral("ITUZ")).toInt();
        record.state = fields.value(QStringLiteral("STATE")).trimmed().toUpper();
        record.county = fields.value(QStringLiteral("CNTY")).trimmed().toUpper();
        if (record.county.isEmpty()) {
            record.county = fields.value(QStringLiteral("COUNTY")).trimmed().toUpper();
        }
        record.lotwConfirmed = adifYes(fields.value(QStringLiteral("LOTW_QSL_RCVD")));
        record.eqslConfirmed = adifYes(fields.value(QStringLiteral("EQSL_QSL_RCVD")));
        record.oqrs = adifYes(fields.value(QStringLiteral("OQRS")));
        record.potaReference =
            normalizedPota(fields.value(QStringLiteral("POTA_REF")));
        if (record.potaReference.isEmpty()
            && fields.value(QStringLiteral("SIG"))
                   .compare(QStringLiteral("POTA"), Qt::CaseInsensitive) == 0) {
            record.potaReference =
                normalizedPota(fields.value(QStringLiteral("SIG_INFO")));
        }
        record.iotaReference =
            normalizedIota(fields.value(QStringLiteral("IOTA")));
        record.wpxPrefix = wpxPrefix(record.call);
        record.confirmed = isConfirmed(fields);
        enrichRecord(record);
        record.sourceKey = digestKey({
            record.call, record.grid, record.band, record.mode,
            record.qsoDate, record.timeOn,
            QString::number(record.frequencyMhz, 'f', 6)
        });
        records.append(std::move(record));
    }
    return records;
}

MapIntelligenceService::LiveSpot
MapIntelligenceService::liveSpotFromEntry(const QVariantMap& entry,
                                          qint64 dialFrequencyHz,
                                          const QString& band)
{
    LiveSpot spot;
    if (entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()) {
        return spot;
    }

    spot.message = entry.value(QStringLiteral("message")).toString().trimmed();
    if (spot.message.isEmpty()) {
        return spot;
    }
    // A decoded standard message carries the transmitting station first.  Do
    // not use a cached dxGrid blindly: it can belong to a previous QSO or to
    // a UI selection and would paint a station in the wrong Maidenhead cell.
    QString const decodedTransmitter = callFromMessage(spot.message);
    spot.call = decodedTransmitter;
    if (spot.call.isEmpty()) {
        spot.call = entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
    }
    if (spot.call.isEmpty()) {
        spot.call = entry.value(QStringLiteral("dxCallsign")).toString().trimmed().toUpper();
    }
    spot.source = entry.value(QStringLiteral("source"), QStringLiteral("decoder"))
                      .toString().trimmed().toLower();

    QString const transmittedGrid = gridFromMessage(spot.message);
    if (!transmittedGrid.isEmpty()
        && ((!decodedTransmitter.isEmpty())
            || messageAssociatesGridWithCall(spot.message, spot.call))) {
        // CQ, standard exchange and beacon messages place the sender's grid
        // in the decoded payload.  This is the only decoder-side location we
        // use for map coverage and grid detail popups.
        spot.grid = transmittedGrid;
    } else if (spot.source == QStringLiteral("psk")
               || spot.source == QStringLiteral("oams")) {
        // External spot feeds provide a station location independently of a
        // decoded over-the-air payload, so their explicit grid remains valid.
        spot.grid = normalizedGrid(entry.value(QStringLiteral("dxGrid")).toString());
    }
    spot.grid4 = spot.grid.left(4);
    spot.grid6 = spot.grid.size() >= 6 ? spot.grid.left(6) : QString();
    spot.mode = normalizedMode(entry.value(QStringLiteral("mode")).toString());
    spot.band = normalizedBand(band, dialFrequencyHz / 1.0e6);
    spot.snr = entry.value(QStringLiteral("db")).toString().toInt();
    spot.frequencyHz = dialFrequencyHz + entry.value(QStringLiteral("freq")).toLongLong();
    spot.distanceKm = entry.value(QStringLiteral("distanceKm"), -1.0).toDouble();
    spot.dxcc = entry.value(QStringLiteral("dxcc")).toString().trimmed();
    spot.continent = entry.value(QStringLiteral("continent")).toString().trimmed().toUpper();
    spot.cqZone = entry.value(QStringLiteral("cqZone")).toInt();
    spot.ituZone = entry.value(QStringLiteral("ituZone")).toInt();
    spot.state = entry.value(QStringLiteral("state")).toString().trimmed().toUpper();
    spot.targetCall = entry.value(QStringLiteral("toCall")).toString().trimmed().toUpper();
    spot.isCq = entry.value(QStringLiteral("isCQ")).toBool()
        || spot.message.compare(QStringLiteral("CQ"), Qt::CaseInsensitive) == 0
        || spot.message.startsWith(QStringLiteral("CQ "), Qt::CaseInsensitive);
    spot.observedMs = entry.value(QStringLiteral("timestamp")).toLongLong();
    if (spot.observedMs <= 0) {
        spot.observedMs = QDateTime::currentMSecsSinceEpoch();
    }
    spot.observedUtc = QDateTime::fromMSecsSinceEpoch(spot.observedMs, QTimeZone::UTC)
                           .toString(Qt::ISODate);
    spot.activityType = activityTypeForMessage(
        spot.message, spot.mode, spot.source, spot.isCq, spot.targetCall);
    spot.uniqueKey = digestKey({
        entry.value(QStringLiteral("time")).toString(),
        spot.call, spot.grid, spot.band, spot.mode,
        QString::number(spot.frequencyHz), spot.message
    });
    return spot;
}

MapIntelligenceService::Snapshot
MapIntelligenceService::queryDatabase(const QString& databasePath,
                                      const QueryOptions& options)
{
    Snapshot snapshot;
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, &snapshot.error)) {
        return snapshot;
    }
    QSqlDatabase& db = connection->database();

    auto scalar = [&db](QString const& sql) {
        QSqlQuery query(db);
        return query.exec(sql) && query.next() ? query.value(0).toInt() : 0;
    };
    snapshot.qsoCount = scalar(QStringLiteral("SELECT COUNT(*) FROM map_qso"));
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const periodCutoff = periodCutoffMs(options.period);
    qint64 const retainedCutoff = nowMs - kLiveRetentionMs;
    qint64 const spotCutoff = qMax(periodCutoff,
                                   qMax(retainedCutoff,
                                        spotAgeCutoff(options.spotAgeFilter, nowMs)));
    // Event history intentionally has a wider retention window than the live
    // spot table.  Do not let the live-table retention silently truncate a
    // user-selected 24 hour or 7 day analytics view.
    qint64 const eventCutoff = qMax(periodCutoff,
                                    spotAgeCutoff(options.spotAgeFilter, nowMs));
    qint64 const coverageCutoff = qMax(
        spotCutoff,
        nowMs - qBound(1, options.liveDecayMinutes, 120) * 60LL * 1000LL);
    QString const qsoGridExpression = options.gridPrecision == 6
        ? QStringLiteral(
              "CASE WHEN length(grid6)=6 THEN upper(grid6) ELSE upper(grid4) END")
        : QStringLiteral("upper(grid4)");
    QString const spotGridExpression = options.gridPrecision == 6
        ? QStringLiteral(
              "CASE WHEN length(grid6)=6 THEN upper(grid6) ELSE upper(grid4) END")
        : QStringLiteral("upper(grid4)");
    bool const allBand =
        options.band.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allMode =
        options.mode.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allContinent =
        options.continent.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allDxcc =
        options.dxcc.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;
    bool const allSource =
        options.source.compare(QStringLiteral("All"), Qt::CaseInsensitive) == 0;

    auto bindCommon = [&](QSqlQuery& query, bool spot) {
        query.bindValue(QStringLiteral(":all_band"), allBand);
        query.bindValue(QStringLiteral(":band"), options.band);
        query.bindValue(QStringLiteral(":all_mode"), allMode);
        query.bindValue(QStringLiteral(":mode"), options.mode);
        query.bindValue(QStringLiteral(":all_continent"), allContinent);
        query.bindValue(QStringLiteral(":continent"), options.continent);
        query.bindValue(QStringLiteral(":all_dxcc"), allDxcc);
        query.bindValue(QStringLiteral(":dxcc"), options.dxcc);
        query.bindValue(QStringLiteral(":all_source"), allSource);
        query.bindValue(QStringLiteral(":source"), options.source);
        query.bindValue(QStringLiteral(":cutoff"), spot ? spotCutoff : periodCutoff);
        if (spot) {
            query.bindValue(QStringLiteral(":cq_only"), options.cqOnly);
        }
    };

    QString const commonFilter = QStringLiteral(
        " AND (:all_band = 1 OR lower(band) = lower(:band))"
        " AND (:all_mode = 1 OR upper(mode) = upper(:mode))"
        " AND (:all_continent = 1 OR upper(continent) = upper(:continent))"
        " AND (:all_dxcc = 1 OR lower(dxcc) = lower(:dxcc))"
        " AND (:all_source = 1 OR lower(source) = lower(:source))");
    QString const qsoFilter =
        commonFilter + QStringLiteral(" AND (:cutoff = 0 OR qso_epoch >= :cutoff)");
    // Awards describe durable logbook progress. A temporary map time window
    // must not make previously earned entities disappear. Keep band/mode
    // scoping, but always evaluate award progress against the full ADIF history.
    QString const awardQsoFilter = QStringLiteral(
        " AND (:all_band = 1 OR lower(band) = lower(:band))"
        " AND (:all_mode = 1 OR upper(mode) = upper(:mode))");
    QString const correlation = options.spotCorrelationFilter.trimmed().toLower();
    QString correlationFilter;
    if (correlation == QStringLiteral("correlated")) {
        correlationFilter = QStringLiteral(" AND correlation_count > 0");
    } else if (correlation == QStringLiteral("local")) {
        correlationFilter = QStringLiteral(" AND lower(source) NOT IN ('psk','oams')");
    } else if (correlation == QStringLiteral("psk reporter")) {
        correlationFilter = QStringLiteral(" AND lower(source)='psk'");
    } else if (correlation == QStringLiteral("oams")) {
        correlationFilter = QStringLiteral(" AND lower(source)='oams'");
    }
    QString const spotFilter =
        commonFilter
        + QStringLiteral(" AND observed_ms >= :cutoff AND (:cq_only = 0 OR is_cq = 1)")
        + (!options.pskLayerEnabled
               ? QStringLiteral(" AND lower(source)<>'psk'")
               : (options.pskDisplayMode.compare(
                      QStringLiteral("Replace"), Qt::CaseInsensitive) == 0
                      ? QStringLiteral(" AND lower(source)='psk'")
                      : QString()))
        + correlationFilter;

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT upper(call)) FROM map_spot"
            " WHERE lower(source)='psk' AND observed_ms >= :cutoff"));
        query.bindValue(QStringLiteral(":cutoff"), nowMs - 60LL * 60LL * 1000LL);
        if (query.exec() && query.next()) {
            snapshot.pskListenerCount = query.value(0).toInt();
        }
    }

    QVariantList activeRosterRules;
    {
        QSqlQuery rulesQuery(db);
        if (rulesQuery.exec(QStringLiteral(
                "SELECT upper(rule_type), upper(rule_value), upper(rule_action),"
                " band, mode FROM map_roster_rule WHERE enabled=1"))) {
            while (rulesQuery.next()) {
                QVariantMap rule;
                rule.insert(QStringLiteral("type"), rulesQuery.value(0).toString());
                rule.insert(QStringLiteral("value"), rulesQuery.value(1).toString());
                rule.insert(QStringLiteral("action"), rulesQuery.value(2).toString());
                rule.insert(QStringLiteral("band"), rulesQuery.value(3).toString());
                rule.insert(QStringLiteral("mode"), rulesQuery.value(4).toString());
                activeRosterRules.append(rule);
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT band FROM map_qso WHERE band <> ''"
                " UNION SELECT DISTINCT band FROM map_spot WHERE band <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            snapshot.bands = sortedBands(values);
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT mode FROM map_qso WHERE mode <> ''"
                " UNION SELECT DISTINCT mode FROM map_spot WHERE mode <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            snapshot.modes = sortedModes(values);
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT continent FROM map_qso WHERE continent <> ''"
                " UNION SELECT DISTINCT continent FROM map_spot WHERE continent <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString().toUpper());
            values.removeDuplicates();
            std::sort(values.begin(), values.end());
            values.prepend(QStringLiteral("All"));
            snapshot.continents = values;
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT dxcc FROM map_qso WHERE dxcc <> ''"
                " UNION SELECT DISTINCT dxcc FROM map_spot WHERE dxcc <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            values.removeDuplicates();
            std::sort(values.begin(), values.end(), [](QString const& a, QString const& b) {
                return a.localeAwareCompare(b) < 0;
            });
            values.prepend(QStringLiteral("All"));
            snapshot.dxcc = values;
        }
    }
    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT DISTINCT source FROM map_qso WHERE source <> ''"
                " UNION SELECT DISTINCT source FROM map_spot WHERE source <> ''"))) {
            QStringList values;
            while (query.next()) values.append(query.value(0).toString());
            values.removeDuplicates();
            std::sort(values.begin(), values.end());
            values.prepend(QStringLiteral("All"));
            snapshot.sources = values;
        }
    }

    QHash<QString, QVariantMap> coverageByGrid;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT %1 AS coverage_grid, COUNT(*), SUM(confirmed) FROM map_qso"
            " WHERE grid4 <> ''").arg(qsoGridExpression) + qsoFilter
            + QStringLiteral(" GROUP BY coverage_grid ORDER BY coverage_grid"));
        bindCommon(query, false);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("grid"), query.value(0).toString());
                row.insert(QStringLiteral("workedCount"), query.value(1).toInt());
                row.insert(QStringLiteral("confirmedCount"), query.value(2).toInt());
                row.insert(QStringLiteral("confirmed"), query.value(2).toInt() > 0);
                row.insert(QStringLiteral("activeCount"), 0);
                row.insert(QStringLiteral("pskCount"), 0);
                row.insert(QStringLiteral("active"), false);
                row.insert(QStringLiteral("missing"), false);
                row.insert(QStringLiteral("psk"), false);
                row.insert(QStringLiteral("historicalStatus"),
                           query.value(2).toInt() > 0
                               ? QStringLiteral("QSL")
                               : QStringLiteral("QSO"));
                row.insert(QStringLiteral("liveStatus"), QString());
                row.insert(QStringLiteral("liveOpacity"), 0.0);
                row.insert(QStringLiteral("split"), false);
                coverageByGrid.insert(query.value(0).toString(), row);
                ++snapshot.workedGridCount;
                if (query.value(2).toInt() > 0) {
                    ++snapshot.confirmedGridCount;
                }
            }
        } else {
            snapshot.error = query.lastError().text();
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT %1 AS coverage_grid, COUNT(*),"
            " SUM(CASE WHEN lower(source)='psk' THEN 1 ELSE 0 END),"
            " MAX(observed_ms),"
            " SUM(CASE WHEN upper(activity_type)='CQ' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='CQDX' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='QRZ' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='WSPR' THEN 1 ELSE 0 END),"
            " SUM(CASE WHEN upper(activity_type)='QSX' THEN 1 ELSE 0 END)"
            " FROM map_spot WHERE grid4 <> ''").arg(spotGridExpression)
            + spotFilter
            + QStringLiteral(" GROUP BY coverage_grid ORDER BY coverage_grid"));
        bindCommon(query, true);
        query.bindValue(QStringLiteral(":cutoff"), coverageCutoff);
        if (query.exec()) {
            while (query.next()) {
                QString const grid = query.value(0).toString();
                QVariantMap row = coverageByGrid.value(grid);
                int const activeCount = query.value(1).toInt();
                int const pskCount = query.value(2).toInt();
                qint64 const newestMs = query.value(3).toLongLong();
                QString liveStatus = QStringLiteral("LIVE");
                if (query.value(5).toInt() > 0) {
                    liveStatus = QStringLiteral("CQDX");
                } else if (query.value(4).toInt() > 0) {
                    liveStatus = QStringLiteral("CQ");
                } else if (query.value(6).toInt() > 0) {
                    liveStatus = QStringLiteral("QRZ");
                } else if (query.value(7).toInt() > 0) {
                    liveStatus = QStringLiteral("WSPR");
                } else if (query.value(8).toInt() > 0) {
                    liveStatus = QStringLiteral("QSX");
                } else if (pskCount > 0) {
                    liveStatus = QStringLiteral("PSK");
                }
                bool const missing = row.isEmpty()
                    || row.value(QStringLiteral("workedCount")).toInt() == 0;
                if (row.isEmpty()) {
                    row.insert(QStringLiteral("grid"), grid);
                    row.insert(QStringLiteral("workedCount"), 0);
                    row.insert(QStringLiteral("confirmedCount"), 0);
                    row.insert(QStringLiteral("confirmed"), false);
                    row.insert(QStringLiteral("historicalStatus"), QString());
                }
                row.insert(QStringLiteral("activeCount"), activeCount);
                row.insert(QStringLiteral("pskCount"), pskCount);
                row.insert(QStringLiteral("active"), activeCount > 0);
                row.insert(QStringLiteral("missing"), missing);
                row.insert(QStringLiteral("psk"), pskCount > 0);
                row.insert(QStringLiteral("lastSeenMs"), newestMs);
                row.insert(QStringLiteral("ageSeconds"),
                           qMax<qint64>(0, nowMs - newestMs) / 1000);
                row.insert(QStringLiteral("liveStatus"), liveStatus);
                qreal opacity = liveOpacityForAge(
                    qMax<qint64>(0, nowMs - newestMs),
                    options.liveDecayMinutes);
                if (liveStatus == QStringLiteral("PSK")) {
                    opacity *= qBound(0.2, options.pskOpacity, 1.0);
                }
                row.insert(QStringLiteral("liveOpacity"), opacity);
                row.insert(QStringLiteral("split"),
                           options.splitGridEnabled
                               && row.value(QStringLiteral("workedCount")).toInt() > 0);
                coverageByGrid.insert(grid, row);
                ++snapshot.activeGridCount;
                if (missing) ++snapshot.missingGridCount;
            }
        }
    }
    QStringList coverageKeys = coverageByGrid.keys();
    std::sort(coverageKeys.begin(), coverageKeys.end());
    for (QString const& grid : std::as_const(coverageKeys)) {
        snapshot.coverage.append(coverageByGrid.value(grid));
    }

    {
        QSqlQuery query(db);
        QString const orderColumn = rosterOrderColumn(options.rosterSort);
        QString const orderDirection =
            options.rosterSortDescending ? QStringLiteral("DESC") : QStringLiteral("ASC");
        QString const scope = options.rosterHuntScope.trimmed().toLower();
        bool const scopeBand = scope != QStringLiteral("all time");
        bool const scopeMode = scope == QStringLiteral("band + mode");
        qint64 const rosterCutoff =
            QDateTime::currentMSecsSinceEpoch()
            - qBound(1, options.rosterRetentionMinutes, 60) * 60LL * 1000LL;
        QString const historyScope = QStringLiteral(
            " AND (:scope_band = 0 OR lower(q.band) = lower(s.band))"
            " AND (:scope_mode = 0 OR upper(q.mode) = upper(s.mode))");
        query.prepare(QStringLiteral(
            "SELECT s.call, s.grid, s.band, s.mode, s.snr, s.frequency_hz,"
            " s.observed_utc, s.source, s.message, s.dxcc, s.continent, s.cq_zone,"
            " s.itu_zone, s.state, s.is_cq, s.target_call, s.distance_km,"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE upper(q.call)=upper(s.call)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE upper(q.call)=upper(s.call) AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.grid4<>'' AND q.grid4=s.grid4") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.grid4<>'' AND q.grid4=s.grid4 AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.dxcc<>'' AND lower(q.dxcc)=lower(s.dxcc)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.dxcc<>'' AND lower(q.dxcc)=lower(s.dxcc)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(
            " LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.cq_zone>0 AND q.cq_zone=s.cq_zone") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.cq_zone>0 AND q.cq_zone=s.cq_zone"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.itu_zone>0 AND q.itu_zone=s.itu_zone") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.itu_zone>0 AND q.itu_zone=s.itu_zone"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.state<>'' AND upper(q.state)=upper(s.state)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.state<>'' AND upper(q.state)=upper(s.state)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(
            " LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.continent<>'' AND upper(q.continent)=upper(s.continent)") + historyScope
            + QStringLiteral(" LIMIT 1),"
            " EXISTS(SELECT 1 FROM map_qso q"
            "   WHERE s.continent<>'' AND upper(q.continent)=upper(s.continent)"
            "     AND q.confirmed=1") + historyScope
            + QStringLiteral(
            " LIMIT 1),"
            " COALESCE(p.watched, 0),"
            " COALESCE(p.ignored, 0)"
            " FROM map_spot s"
            " LEFT JOIN map_roster_preference p ON upper(p.call)=upper(s.call)"
            " WHERE COALESCE(p.ignored, 0)=0"
            " AND NOT EXISTS("
            "   SELECT 1 FROM map_roster_ignore i"
            "   WHERE upper(i.ignore_type)='DXCC'"
            "     AND upper(i.ignore_value)=upper(s.dxcc))"
            " AND s.id IN ("
            "   SELECT MAX(recent.id) FROM map_spot recent"
            "   WHERE recent.observed_ms >= :roster_cutoff"
            "     AND recent.call <> ''"
            "     AND lower(recent.source) <> 'psk'"
            "     AND (:roster_cq_only = 0 OR recent.is_cq = 1)"
            "   GROUP BY upper(recent.call))"
            " ORDER BY COALESCE(p.watched, 0) DESC,"
            " %1 %2, s.observed_ms DESC LIMIT :limit")
                  .arg(orderColumn, orderDirection));
        query.bindValue(QStringLiteral(":scope_band"), scopeBand);
        query.bindValue(QStringLiteral(":scope_mode"), scopeMode);
        query.bindValue(QStringLiteral(":roster_cutoff"), rosterCutoff);
        query.bindValue(QStringLiteral(":roster_cq_only"), options.rosterCqOnly);
        query.bindValue(QStringLiteral(":limit"), kRosterCandidateLimit);
        if (query.exec()) {
            QSqlQuery profileQuery(db);
            profileQuery.prepare(QStringLiteral(
                "SELECT COALESCE(MAX(county), ''), COALESCE(MAX(pota_ref), ''),"
                " COALESCE(MAX(iota), ''), COALESCE(MAX(wpx), ''),"
                " COALESCE(MAX(qso_epoch), 0),"
                " COALESCE(MAX(CASE WHEN lotw_confirmed=1 THEN qso_epoch ELSE 0 END), 0),"
                " COALESCE(MAX(CASE WHEN eqsl_confirmed=1 THEN qso_epoch ELSE 0 END), 0),"
                " COALESCE(MAX(oqrs), 0)"
                " FROM map_qso WHERE upper(call)=upper(:call)"));
            while (query.next()) {
                bool const callWorked = query.value(17).toBool();
                bool const callConfirmed = query.value(18).toBool();
                bool const gridWorked = query.value(19).toBool();
                bool const gridConfirmed = query.value(20).toBool();
                bool const dxccWorked = query.value(21).toBool();
                bool const dxccConfirmed = query.value(22).toBool();
                bool const cqWorked = query.value(23).toBool();
                bool const cqConfirmed = query.value(24).toBool();
                bool const ituWorked = query.value(25).toBool();
                bool const ituConfirmed = query.value(26).toBool();
                bool const stateWorked = query.value(27).toBool();
                bool const stateConfirmed = query.value(28).toBool();
                bool const continentWorked = query.value(29).toBool();
                bool const continentConfirmed = query.value(30).toBool();
                bool watched = query.value(31).toBool();
                bool const ignored = query.value(32).toBool();
                bool const hasGrid = !query.value(1).toString().trimmed().isEmpty();
                bool const hasDxcc = !query.value(9).toString().trimmed().isEmpty();
                QString county;
                QString pota;
                QString iota;
                QString wpx;
                qint64 lastQsoEpoch = 0;
                qint64 lotwEpoch = 0;
                qint64 eqslEpoch = 0;
                bool oqrs = false;
                profileQuery.bindValue(QStringLiteral(":call"), query.value(0).toString());
                if (profileQuery.exec() && profileQuery.next()) {
                    county = profileQuery.value(0).toString().trimmed().toUpper();
                    pota = profileQuery.value(1).toString().trimmed().toUpper();
                    iota = profileQuery.value(2).toString().trimmed().toUpper();
                    wpx = profileQuery.value(3).toString().trimmed().toUpper();
                    lastQsoEpoch = profileQuery.value(4).toLongLong();
                    lotwEpoch = profileQuery.value(5).toLongLong();
                    eqslEpoch = profileQuery.value(6).toLongLong();
                    oqrs = profileQuery.value(7).toBool();
                }
                auto ageDays = [nowMs](qint64 epoch) {
                    return epoch > 0 ? static_cast<int>(qMax<qint64>(0, nowMs - epoch)
                                                         / (24LL * 60LL * 60LL * 1000LL))
                                     : -1;
                };
                int const lotwAgeDays = ageDays(lotwEpoch);
                int const eqslAgeDays = ageDays(eqslEpoch);
                int const lastQsoAgeDays = ageDays(lastQsoEpoch);
                QString const rosterNeedle = options.rosterText.trimmed();
                QString const rosterMode = options.rosterTextMode.trimmed().toLower();
                if (!rosterNeedle.isEmpty()
                    && rosterMode != QStringLiteral("no filter")) {
                    QString const searchable =
                        QStringLiteral("%1 %2 %3 %4 %5 %6 %7")
                            .arg(query.value(0).toString(),
                                 query.value(1).toString(),
                                 query.value(8).toString(),
                                 query.value(9).toString(),
                                 query.value(10).toString(),
                                 query.value(13).toString(),
                                 query.value(15).toString());
                    bool matches = false;
                    if (rosterMode == QStringLiteral("regex")) {
                        QRegularExpression const expression(
                            rosterNeedle,
                            QRegularExpression::CaseInsensitiveOption);
                        matches = expression.isValid()
                            && expression.match(searchable).hasMatch();
                    } else {
                        matches = searchable.contains(rosterNeedle,
                                                      Qt::CaseInsensitive);
                    }
                    if ((rosterMode == QStringLiteral("exclude") && matches)
                        || (rosterMode != QStringLiteral("exclude") && !matches)) {
                        continue;
                    }
                }
                bool const anyNew =
                    !callWorked || (hasGrid && !gridWorked) || (hasDxcc && !dxccWorked);
                bool const hasUnconfirmedEntity =
                    (callWorked && !callConfirmed)
                    || (hasGrid && gridWorked && !gridConfirmed)
                    || (hasDxcc && dxccWorked && !dxccConfirmed);
                bool const anyUnconfirmed = !anyNew && hasUnconfirmedEntity;
                QString const activeAward =
                    options.activeAwardProgram.trimmed().toLower();
                bool const confirmedGoal =
                    options.awardGoal.compare(QStringLiteral("Confirmed"),
                                              Qt::CaseInsensitive) == 0;
                bool awardEligible = false;
                bool awardWorked = false;
                bool awardConfirmed = false;
                QString awardEntity;
                if (activeAward == QStringLiteral("dxcc")) {
                    awardEligible = hasDxcc;
                    awardWorked = dxccWorked;
                    awardConfirmed = dxccConfirmed;
                    awardEntity = query.value(9).toString();
                } else if (activeAward == QStringLiteral("maidenhead")) {
                    awardEligible = hasGrid;
                    awardWorked = gridWorked;
                    awardConfirmed = gridConfirmed;
                    awardEntity = query.value(1).toString().left(4);
                } else if (activeAward == QStringLiteral("waz")) {
                    int const zone = query.value(11).toInt();
                    awardEligible = zone >= 1 && zone <= 40;
                    awardWorked = cqWorked;
                    awardConfirmed = cqConfirmed;
                    awardEntity = QString::number(zone);
                } else if (activeAward == QStringLiteral("was")) {
                    awardEntity = query.value(13).toString().trimmed().toUpper();
                    awardEligible = isWasState(awardEntity);
                    awardWorked = stateWorked;
                    awardConfirmed = stateConfirmed;
                } else if (activeAward == QStringLiteral("us48")) {
                    awardEntity = query.value(13).toString().trimmed().toUpper();
                    awardEligible = isLower48State(awardEntity);
                    awardWorked = stateWorked;
                    awardConfirmed = stateConfirmed;
                } else if (activeAward == QStringLiteral("wac")) {
                    awardEntity = query.value(10).toString().trimmed().toUpper();
                    awardEligible = QStringList {
                        QStringLiteral("AF"), QStringLiteral("AS"),
                        QStringLiteral("EU"), QStringLiteral("NA"),
                        QStringLiteral("OC"), QStringLiteral("SA")
                    }.contains(awardEntity);
                    awardWorked = continentWorked;
                    awardConfirmed = continentConfirmed;
                } else if (activeAward == QStringLiteral("itu zones")) {
                    int const zone = query.value(12).toInt();
                    awardEligible = zone >= 1 && zone <= 90;
                    awardWorked = ituWorked;
                    awardConfirmed = ituConfirmed;
                    awardEntity = QString::number(zone);
                } else if (ExternalAwardDefinition const* external =
                               externalAwardForLabel(options.activeAwardProgram)) {
                    awardEntity = externalAwardSpotEntity(
                        *external,
                        query.value(0).toString(), query.value(2).toString(),
                        query.value(1).toString(),
                        query.value(9).toString(), query.value(11).toInt(),
                        query.value(13).toString(), query.value(10).toString(),
                        county, iota);
                    awardEligible = !awardEntity.isEmpty()
                        && externalAwardMatchesSpot(*external,
                                                    query.value(2).toString(),
                                                    query.value(3).toString());
                    if (external->type == QStringLiteral("grids")) {
                        awardWorked = gridWorked;
                        awardConfirmed = gridConfirmed;
                    } else if (external->type == QStringLiteral("dxcc")
                               || external->type == QStringLiteral("dxcc2band")
                               || external->type == QStringLiteral("calls2dxcc")) {
                        awardWorked = dxccWorked;
                        awardConfirmed = dxccConfirmed;
                    } else if (external->type == QStringLiteral("cqz")) {
                        awardWorked = cqWorked;
                        awardConfirmed = cqConfirmed;
                    } else if (external->type == QStringLiteral("states")
                               || external->type == QStringLiteral("states2band")) {
                        awardWorked = stateWorked;
                        awardConfirmed = stateConfirmed;
                    } else if (external->type == QStringLiteral("cont")
                               || external->type == QStringLiteral("cont2band")
                               || external->type == QStringLiteral("cont5")
                               || external->type == QStringLiteral("cont52band")) {
                        awardWorked = continentWorked;
                        awardConfirmed = continentConfirmed;
                    } else {
                        awardWorked = callWorked;
                        awardConfirmed = callConfirmed;
                    }
                }
                bool const awardMode = activeAward != QStringLiteral("none")
                    && !activeAward.isEmpty();
                bool const awardWanted = awardMode && awardEligible
                    && (confirmedGoal ? !awardConfirmed : !awardWorked);
                bool forceWanted = false;
                bool ignoredByRule = false;
                QStringList ruleReasons;
                for (QVariant const& variant : activeRosterRules) {
                    QVariantMap const rule = variant.toMap();
                    QString const ruleBand = rule.value(QStringLiteral("band")).toString();
                    QString const ruleMode = rule.value(QStringLiteral("mode")).toString();
                    if (!ruleBand.isEmpty()
                        && ruleBand.compare(query.value(2).toString(), Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    if (!ruleMode.isEmpty()
                        && ruleMode.compare(query.value(3).toString(), Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    QString const type = rule.value(QStringLiteral("type")).toString();
                    QString candidate;
                    if (type == QStringLiteral("CALL")) candidate = query.value(0).toString();
                    else if (type == QStringLiteral("GRID")) candidate = query.value(1).toString().left(4);
                    else if (type == QStringLiteral("DXCC")) candidate = query.value(9).toString();
                    else if (type == QStringLiteral("WPX")) candidate = wpx;
                    else if (type == QStringLiteral("CQ")) candidate = QString::number(query.value(11).toInt());
                    else if (type == QStringLiteral("ITU")) candidate = QString::number(query.value(12).toInt());
                    else if (type == QStringLiteral("STATE")) candidate = query.value(13).toString();
                    else if (type == QStringLiteral("CONTINENT")) candidate = query.value(10).toString();
                    else if (type == QStringLiteral("COUNTY")) candidate = county;
                    else if (type == QStringLiteral("POTA")) candidate = pota;
                    else if (type == QStringLiteral("IOTA")) candidate = iota;
                    else if (type == QStringLiteral("OQRS")) candidate = oqrs ? QStringLiteral("YES") : QStringLiteral("NO");
                    else if (type == QStringLiteral("BAND")) candidate = query.value(2).toString();
                    else if (type == QStringLiteral("MODE")) candidate = query.value(3).toString();
                    if (candidate.isEmpty()
                        || candidate.compare(rule.value(QStringLiteral("value")).toString(),
                                             Qt::CaseInsensitive) != 0) {
                        continue;
                    }
                    QString const action = rule.value(QStringLiteral("action")).toString();
                    if (action == QStringLiteral("IGNORE")) {
                        ignoredByRule = true;
                        break;
                    }
                    if (action == QStringLiteral("WATCH")) {
                        watched = true;
                        ruleReasons.append(QStringLiteral("Watch rule"));
                    } else if (action == QStringLiteral("WANTED")) {
                        forceWanted = true;
                        ruleReasons.append(QStringLiteral("Wanted rule"));
                    }
                }
                if (ignoredByRule) {
                    continue;
                }
                bool const wanted = forceWanted || (awardMode
                    ? awardWanted
                    : (anyNew || hasUnconfirmedEntity));

                if (anyNew) ++snapshot.rosterNewCount;
                if (anyUnconfirmed) ++snapshot.rosterUnconfirmedCount;
                if (wanted) ++snapshot.rosterWantedCount;

                QString const statusFilter = options.rosterStatus.trimmed().toLower();
                if ((statusFilter == QStringLiteral("new") && !anyNew)
                    || (statusFilter == QStringLiteral("unconfirmed") && !anyUnconfirmed)
                    || (statusFilter == QStringLiteral("wanted") && !wanted)
                    || (statusFilter == QStringLiteral("award") && !awardWanted)
                    || (statusFilter == QStringLiteral("watched") && !watched)) {
                    continue;
                }

                QStringList reasons;
                if (hasDxcc && !dxccWorked) reasons.append(QStringLiteral("New DXCC"));
                if (hasGrid && !gridWorked) reasons.append(QStringLiteral("New grid"));
                if (!callWorked) reasons.append(QStringLiteral("New call"));
                if (hasDxcc && dxccWorked && !dxccConfirmed) {
                    reasons.append(QStringLiteral("DXCC unconfirmed"));
                }
                if (hasGrid && gridWorked && !gridConfirmed) {
                    reasons.append(QStringLiteral("Grid unconfirmed"));
                }
                if (callWorked && !callConfirmed) {
                    reasons.append(QStringLiteral("Call unconfirmed"));
                }
                if (awardWanted) {
                    reasons.prepend(QStringLiteral("%1 %2 %3")
                                        .arg(options.activeAwardProgram,
                                             awardEntity,
                                             confirmedGoal
                                                 ? QStringLiteral("unconfirmed")
                                                 : QStringLiteral("new")));
                }
                reasons.append(ruleReasons);

                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("snr"), query.value(4).toInt());
                row.insert(QStringLiteral("frequencyHz"), query.value(5).toLongLong());
                row.insert(QStringLiteral("observedUtc"), query.value(6).toString());
                row.insert(QStringLiteral("source"), query.value(7).toString());
                row.insert(QStringLiteral("message"), query.value(8).toString());
                row.insert(QStringLiteral("dxcc"), query.value(9).toString());
                row.insert(QStringLiteral("continent"), query.value(10).toString());
                row.insert(QStringLiteral("cqZone"), query.value(11).toInt());
                row.insert(QStringLiteral("ituZone"), query.value(12).toInt());
                row.insert(QStringLiteral("state"), query.value(13).toString());
                row.insert(QStringLiteral("county"), county);
                row.insert(QStringLiteral("pota"), pota);
                row.insert(QStringLiteral("iota"), iota);
                row.insert(QStringLiteral("wpx"), wpx);
                row.insert(QStringLiteral("lotwAgeDays"), lotwAgeDays);
                row.insert(QStringLiteral("eqslAgeDays"), eqslAgeDays);
                row.insert(QStringLiteral("oqrs"), oqrs);
                row.insert(QStringLiteral("ageDays"), lastQsoAgeDays);
                row.insert(QStringLiteral("isCQ"), query.value(14).toBool());
                row.insert(QStringLiteral("targetCall"), query.value(15).toString());
                row.insert(QStringLiteral("distanceKm"), query.value(16).toDouble());
                row.insert(QStringLiteral("callWorked"), callWorked);
                row.insert(QStringLiteral("callConfirmed"), callConfirmed);
                row.insert(QStringLiteral("gridWorked"), gridWorked);
                row.insert(QStringLiteral("gridConfirmed"), gridConfirmed);
                row.insert(QStringLiteral("dxccWorked"), dxccWorked);
                row.insert(QStringLiteral("dxccConfirmed"), dxccConfirmed);
                row.insert(QStringLiteral("new"), anyNew);
                row.insert(QStringLiteral("unconfirmed"), anyUnconfirmed);
                row.insert(QStringLiteral("wanted"), wanted);
                row.insert(QStringLiteral("awardWanted"), awardWanted);
                row.insert(QStringLiteral("awardProgram"),
                           options.activeAwardProgram);
                row.insert(QStringLiteral("awardEntity"), awardEntity);
                row.insert(QStringLiteral("awardWorked"), awardWorked);
                row.insert(QStringLiteral("awardConfirmed"), awardConfirmed);
                row.insert(QStringLiteral("watched"), watched);
                row.insert(QStringLiteral("ignored"), ignored);
                row.insert(QStringLiteral("status"),
                           anyNew ? QStringLiteral("NEW")
                                  : (anyUnconfirmed ? QStringLiteral("UNCONFIRMED")
                                                    : QStringLiteral("CONFIRMED")));
                row.insert(QStringLiteral("huntReason"), reasons.mid(0, 2).join(QStringLiteral(" · ")));
                snapshot.roster.append(row);
                if (snapshot.roster.size() >= kRosterLimit) break;
            }
        } else if (snapshot.error.isEmpty()) {
            snapshot.error = query.lastError().text();
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT COUNT(*) FROM map_spot WHERE 1=1") + spotFilter);
        bindCommon(query, true);
        if (query.exec() && query.next()) {
            snapshot.liveSpotCount = query.value(0).toInt();
        }
    }

    // Keep the event log separate from current spot state. It permits a
    // temporal heatmap and directional paths without growing the live roster.
    QString eventFilter = QStringLiteral(
        " WHERE observed_ms >= :event_cutoff"
        " AND (:event_all_band = 1 OR lower(band)=lower(:event_band))"
        " AND (:event_all_mode = 1 OR upper(mode)=upper(:event_mode))"
        " AND (:event_all_source = 1 OR lower(source)=lower(:event_source))"
        " AND (:event_cq_only = 0 OR upper(activity_type) IN ('CQ','CQDX','QRZ'))");
    QString const eventCorrelation = options.spotCorrelationFilter.trimmed().toLower();
    if (eventCorrelation == QStringLiteral("correlated")) {
        eventFilter += QStringLiteral(" AND correlation > 0");
    } else if (eventCorrelation == QStringLiteral("local")) {
        eventFilter += QStringLiteral(" AND lower(source) NOT IN ('psk','oams')");
    } else if (eventCorrelation == QStringLiteral("psk reporter")) {
        eventFilter += QStringLiteral(" AND lower(source)='psk'");
    } else if (eventCorrelation == QStringLiteral("oams")) {
        eventFilter += QStringLiteral(" AND lower(source)='oams'");
    }
    auto bindEvent = [&](QSqlQuery& query) {
        query.bindValue(QStringLiteral(":event_cutoff"), eventCutoff);
        query.bindValue(QStringLiteral(":event_all_band"), allBand);
        query.bindValue(QStringLiteral(":event_band"), options.band);
        query.bindValue(QStringLiteral(":event_all_mode"), allMode);
        query.bindValue(QStringLiteral(":event_mode"), options.mode);
        query.bindValue(QStringLiteral(":event_all_source"), allSource);
        query.bindValue(QStringLiteral(":event_source"), options.source);
        query.bindValue(QStringLiteral(":event_cq_only"), options.cqOnly);
    };
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT upper(substr(grid,1,4)), COUNT(*), MAX(observed_ms),"
            " ROUND(AVG(snr), 1), COUNT(DISTINCT NULLIF(receiver_call, '')) ,"
            " COALESCE(SUM(CASE WHEN correlation>0 THEN 1 ELSE 0 END), 0)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " AND grid<>'' GROUP BY upper(substr(grid,1,4))"
                          " ORDER BY COUNT(*) DESC, MAX(observed_ms) DESC LIMIT 2000"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("grid"), query.value(0).toString());
                row.insert(QStringLiteral("count"), query.value(1).toInt());
                row.insert(QStringLiteral("lastObservedMs"), query.value(2).toLongLong());
                row.insert(QStringLiteral("averageSnr"), query.value(3).toDouble());
                row.insert(QStringLiteral("receivers"), query.value(4).toInt());
                row.insert(QStringLiteral("correlated"), query.value(5).toInt());
                snapshot.spotHeatmap.append(row);
            }
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT (observed_ms / 300000) * 300000, lower(source), COUNT(*),"
            " COUNT(DISTINCT upper(call)), COALESCE(SUM(CASE WHEN correlation>0 THEN 1 ELSE 0 END), 0)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " GROUP BY 1, 2 ORDER BY 1 DESC, 2 LIMIT 576"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("bucketMs"), query.value(0).toLongLong());
                row.insert(QStringLiteral("source"), query.value(1).toString());
                row.insert(QStringLiteral("count"), query.value(2).toInt());
                row.insert(QStringLiteral("calls"), query.value(3).toInt());
                row.insert(QStringLiteral("correlated"), query.value(4).toInt());
                snapshot.spotTimeline.append(row);
            }
        }
    }
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT receiver_grid, grid, receiver_call, call, COUNT(*),"
            " MAX(observed_ms), ROUND(AVG(correlation), 2), lower(source)"
            " FROM map_spot_event")
                      + eventFilter
                      + QStringLiteral(
                          " AND receiver_grid<>'' AND grid<>''"
                          " GROUP BY receiver_grid, grid, receiver_call, call, lower(source)"
                          " ORDER BY MAX(observed_ms) DESC LIMIT 250"));
        bindEvent(query);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("fromGrid"), query.value(0).toString());
                row.insert(QStringLiteral("toGrid"), query.value(1).toString());
                row.insert(QStringLiteral("fromCall"), query.value(2).toString());
                row.insert(QStringLiteral("toCall"), query.value(3).toString());
                row.insert(QStringLiteral("count"), query.value(4).toInt());
                row.insert(QStringLiteral("lastObservedMs"), query.value(5).toLongLong());
                row.insert(QStringLiteral("correlation"), query.value(6).toDouble());
                row.insert(QStringLiteral("source"), query.value(7).toString());
                snapshot.spotPaths.append(row);
            }
        }
    }

    // Blend retained spot history into the GPU coverage model.  This keeps the
    // heatmap a first-class map layer instead of a sidebar-only statistic.
    // Existing grid state wins for QSO/QSL data; the PSK fields add intensity
    // and recency without overwriting that history.
    if (!snapshot.spotHeatmap.isEmpty()) {
        QHash<QString, int> coverageByGrid;
        coverageByGrid.reserve(snapshot.coverage.size());
        for (int index = 0; index < snapshot.coverage.size(); ++index) {
            QString const grid = snapshot.coverage.at(index).toMap()
                                     .value(QStringLiteral("grid")).toString()
                                     .trimmed().toUpper();
            if (!grid.isEmpty()) coverageByGrid.insert(grid, index);
        }
        for (QVariant const& value : std::as_const(snapshot.spotHeatmap)) {
            QVariantMap const heat = value.toMap();
            QString const grid = heat.value(QStringLiteral("grid")).toString()
                                     .trimmed().toUpper();
            if (grid.size() < 4) continue;
            int const count = heat.value(QStringLiteral("count")).toInt();
            qint64 const lastObservedMs = heat.value(QStringLiteral("lastObservedMs")).toLongLong();
            double const intensity = qBound(0.18,
                0.20 + qLn(1.0 + qMax(0, count)) / 4.0, 1.0);
            auto applyHeat = [&](QVariantMap& coverage) {
                coverage.insert(QStringLiteral("psk"), true);
                coverage.insert(QStringLiteral("pskCount"), count);
                coverage.insert(QStringLiteral("heatCount"), count);
                coverage.insert(QStringLiteral("heatLastObservedMs"), lastObservedMs);
                coverage.insert(QStringLiteral("heatAverageSnr"),
                                heat.value(QStringLiteral("averageSnr")));
                coverage.insert(QStringLiteral("heatReceivers"),
                                heat.value(QStringLiteral("receivers")));
                coverage.insert(QStringLiteral("liveOpacity"), qMax(
                                    coverage.value(QStringLiteral("liveOpacity"), 0.0).toDouble(),
                                    intensity));
            };
            auto const existing = coverageByGrid.constFind(grid);
            if (existing != coverageByGrid.constEnd()) {
                QVariantMap coverage = snapshot.coverage.at(existing.value()).toMap();
                applyHeat(coverage);
                snapshot.coverage[existing.value()] = coverage;
            } else {
                QVariantMap coverage;
                coverage.insert(QStringLiteral("grid"), grid);
                coverage.insert(QStringLiteral("worked"), false);
                coverage.insert(QStringLiteral("confirmed"), false);
                coverage.insert(QStringLiteral("active"), false);
                coverage.insert(QStringLiteral("missing"), false);
                coverage.insert(QStringLiteral("split"), false);
                coverage.insert(QStringLiteral("liveCount"), 0);
                coverage.insert(QStringLiteral("liveStatus"), QStringLiteral("PSK"));
                applyHeat(coverage);
                coverageByGrid.insert(grid, snapshot.coverage.size());
                snapshot.coverage.append(coverage);
            }
        }
    }

    {
        QVariantMap statistics;
        {
            QSqlQuery total(db);
            if (total.exec(QStringLiteral(
                    "SELECT COUNT(*), COALESCE(SUM(confirmed), 0),"
                    " COUNT(DISTINCT CASE WHEN call<>'' THEN upper(call) END),"
                    " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN lower(dxcc) END),"
                    " COUNT(DISTINCT CASE WHEN grid4<>'' THEN upper(grid4) END),"
                    " MIN(qso_epoch), MAX(qso_epoch)"
                    " FROM map_qso"))) {
                if (total.next()) {
                    statistics.insert(QStringLiteral("totalQso"), total.value(0).toInt());
                    statistics.insert(QStringLiteral("totalConfirmed"),
                                      total.value(1).toInt());
                    statistics.insert(QStringLiteral("totalCalls"), total.value(2).toInt());
                    statistics.insert(QStringLiteral("totalDxcc"), total.value(3).toInt());
                    statistics.insert(QStringLiteral("totalGrids"), total.value(4).toInt());
                    statistics.insert(QStringLiteral("totalFirstEpoch"),
                                      total.value(5).toLongLong());
                    statistics.insert(QStringLiteral("totalLastEpoch"),
                                      total.value(6).toLongLong());
                }
            }
        }
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(confirmed), 0),"
            " COUNT(DISTINCT CASE WHEN call<>'' THEN upper(call) END),"
            " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN lower(dxcc) END),"
            " COUNT(DISTINCT CASE WHEN grid4<>'' THEN upper(grid4) END),"
            " MIN(qso_epoch), MAX(qso_epoch)"
            " FROM map_qso WHERE 1=1") + qsoFilter);
        bindCommon(query, false);
        if (query.exec() && query.next()) {
            statistics.insert(QStringLiteral("qso"), query.value(0).toInt());
            statistics.insert(QStringLiteral("confirmed"), query.value(1).toInt());
            statistics.insert(QStringLiteral("calls"), query.value(2).toInt());
            statistics.insert(QStringLiteral("dxcc"), query.value(3).toInt());
            statistics.insert(QStringLiteral("grids"), query.value(4).toInt());
            statistics.insert(QStringLiteral("firstEpoch"), query.value(5).toLongLong());
            statistics.insert(QStringLiteral("lastEpoch"), query.value(6).toLongLong());
        }

        auto groupedRows = [&db, &bindCommon, &qsoFilter](QString const& column) {
            QVariantList rows;
            QSqlQuery grouped(db);
            grouped.prepare(QStringLiteral(
                "SELECT COALESCE(NULLIF(%1, ''), 'Unknown'), COUNT(*),"
                " COALESCE(SUM(confirmed), 0)"
                " FROM map_qso WHERE 1=1")
                                .arg(column)
                            + qsoFilter
                            + QStringLiteral(
                                " GROUP BY 1 ORDER BY COUNT(*) DESC, 1 LIMIT 12"));
            bindCommon(grouped, false);
            if (grouped.exec()) {
                while (grouped.next()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("label"), grouped.value(0).toString());
                    row.insert(QStringLiteral("qso"), grouped.value(1).toInt());
                    row.insert(QStringLiteral("confirmed"), grouped.value(2).toInt());
                    rows.append(row);
                }
            }
            return rows;
        };
        statistics.insert(QStringLiteral("bands"), groupedRows(QStringLiteral("band")));
        statistics.insert(QStringLiteral("modes"), groupedRows(QStringLiteral("mode")));
        statistics.insert(QStringLiteral("continents"),
                          groupedRows(QStringLiteral("continent")));
        statistics.insert(QStringLiteral("live"), snapshot.liveSpotCount);
        statistics.insert(QStringLiteral("roster"), snapshot.roster.size());
        statistics.insert(QStringLiteral("spotHeatmap"), snapshot.spotHeatmap.size());
        statistics.insert(QStringLiteral("spotTimeline"), snapshot.spotTimeline.size());
        statistics.insert(QStringLiteral("spotPaths"), snapshot.spotPaths.size());
        statistics.insert(QStringLiteral("awardCatalogCount"), externalAwardDefinitions().size());
        statistics.insert(QStringLiteral("period"), options.period);
        statistics.insert(QStringLiteral("band"), options.band);
        statistics.insert(QStringLiteral("mode"), options.mode);
        snapshot.statistics = statistics;
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT call, watched, ignored, updated_ms"
                " FROM map_roster_preference"
                " WHERE watched=1 OR ignored=1"
                " ORDER BY updated_ms DESC"))) {
            while (query.next()) {
                QString const call = query.value(0).toString();
                qint64 const updatedMs = query.value(3).toLongLong();
                if (query.value(1).toBool()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("type"), QStringLiteral("WATCH"));
                    row.insert(QStringLiteral("value"), call);
                    row.insert(QStringLiteral("updatedMs"), updatedMs);
                    snapshot.rosterPreferences.append(row);
                }
                if (query.value(2).toBool()) {
                    QVariantMap row;
                    row.insert(QStringLiteral("type"), QStringLiteral("CALL"));
                    row.insert(QStringLiteral("value"), call);
                    row.insert(QStringLiteral("updatedMs"), updatedMs);
                    snapshot.rosterPreferences.append(row);
                }
            }
        }
        if (query.exec(QStringLiteral(
                "SELECT upper(ignore_type), ignore_value, updated_ms"
                " FROM map_roster_ignore ORDER BY updated_ms DESC"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("value"), query.value(1).toString());
                row.insert(QStringLiteral("updatedMs"), query.value(2).toLongLong());
                snapshot.rosterPreferences.append(row);
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT rule_type, rule_value, rule_action, band, mode, enabled, updated_ms"
                " FROM map_roster_rule ORDER BY rule_type, rule_value, band, mode"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("value"), query.value(1).toString());
                row.insert(QStringLiteral("action"), query.value(2).toString());
                row.insert(QStringLiteral("band"), query.value(3).toString());
                row.insert(QStringLiteral("mode"), query.value(4).toString());
                row.insert(QStringLiteral("enabled"), query.value(5).toBool());
                row.insert(QStringLiteral("updatedMs"), query.value(6).toLongLong());
                snapshot.rosterRules.append(row);
            }
        }
    }

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT"
            " COUNT(DISTINCT CASE WHEN dxcc<>'' THEN dxcc END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND dxcc<>'' THEN dxcc END),"
            " COUNT(DISTINCT CASE WHEN grid4<>'' THEN grid4 END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND grid4<>'' THEN grid4 END),"
            " COUNT(DISTINCT CASE WHEN cq_zone>0 THEN cq_zone END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND cq_zone>0 THEN cq_zone END),"
            " COUNT(DISTINCT CASE WHEN upper(state) IN ("
            "'AL','AK','AZ','AR','CA','CO','CT','DE','FL','GA','HI','ID',"
            "'IL','IN','IA','KS','KY','LA','ME','MD','MA','MI','MN','MS',"
            "'MO','MT','NE','NV','NH','NJ','NM','NY','NC','ND','OH','OK',"
            "'OR','PA','RI','SC','SD','TN','TX','UT','VT','VA','WA','WV',"
            "'WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(state) IN ("
            "'AL','AK','AZ','AR','CA','CO','CT','DE','FL','GA','HI','ID',"
            "'IL','IN','IA','KS','KY','LA','ME','MD','MA','MI','MN','MS',"
            "'MO','MT','NE','NV','NH','NJ','NM','NY','NC','ND','OH','OK',"
            "'OR','PA','RI','SC','SD','TN','TX','UT','VT','VA','WA','WV',"
            "'WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN itu_zone>0 THEN itu_zone END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND itu_zone>0 THEN itu_zone END),"
            " COUNT(DISTINCT CASE WHEN upper(state) IN ("
            "'AL','AZ','AR','CA','CO','CT','DE','FL','GA','ID','IL','IN','IA',"
            "'KS','KY','LA','ME','MD','MA','MI','MN','MS','MO','MT','NE','NV',"
            "'NH','NJ','NM','NY','NC','ND','OH','OK','OR','PA','RI','SC','SD',"
            "'TN','TX','UT','VT','VA','WA','WV','WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(state) IN ("
            "'AL','AZ','AR','CA','CO','CT','DE','FL','GA','ID','IL','IN','IA',"
            "'KS','KY','LA','ME','MD','MA','MI','MN','MS','MO','MT','NE','NV',"
            "'NH','NJ','NM','NY','NC','ND','OH','OK','OR','PA','RI','SC','SD',"
            "'TN','TX','UT','VT','VA','WA','WV','WI','WY') THEN upper(state) END),"
            " COUNT(DISTINCT CASE WHEN upper(continent) IN "
            "('AF','AS','EU','NA','OC','SA') THEN upper(continent) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND upper(continent) IN "
            "('AF','AS','EU','NA','OC','SA') THEN upper(continent) END),"
            " COUNT(DISTINCT CASE WHEN pota_ref<>'' THEN upper(pota_ref) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND pota_ref<>''"
            " THEN upper(pota_ref) END),"
            " COUNT(DISTINCT CASE WHEN iota<>'' THEN upper(iota) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND iota<>''"
            " THEN upper(iota) END),"
            " COUNT(DISTINCT CASE WHEN wpx<>'' THEN upper(wpx) END),"
            " COUNT(DISTINCT CASE WHEN confirmed=1 AND wpx<>''"
            " THEN upper(wpx) END)"
            " FROM map_qso WHERE 1=1") + awardQsoFilter);
        bindCommon(query, false);
        if (query.exec() && query.next()) {
            bool const confirmedGoal =
                options.awardGoal.compare(QStringLiteral("Confirmed"),
                                          Qt::CaseInsensitive) == 0;
            auto addAward = [&snapshot, &options, confirmedGoal](
                                QString const& id, QString const& label,
                                int worked, int confirmed, int target,
                                QString const& rule) {
                int const achieved = confirmedGoal ? confirmed : worked;
                QVariantMap award;
                award.insert(QStringLiteral("id"), id);
                award.insert(QStringLiteral("label"), label);
                award.insert(QStringLiteral("worked"), worked);
                award.insert(QStringLiteral("confirmed"), confirmed);
                award.insert(QStringLiteral("target"), target);
                award.insert(QStringLiteral("achieved"), achieved);
                award.insert(QStringLiteral("remaining"), qMax(0, target - achieved));
                award.insert(QStringLiteral("complete"), achieved >= target);
                award.insert(QStringLiteral("selected"),
                             options.activeAwardProgram.compare(
                                 label, Qt::CaseInsensitive) == 0);
                award.insert(QStringLiteral("goal"), options.awardGoal);
                award.insert(QStringLiteral("scope"),
                             QStringLiteral("%1 / %2 / All time")
                                 .arg(options.band, options.mode));
                award.insert(QStringLiteral("rule"), rule);
                award.insert(QStringLiteral("progress"),
                             target > 0
                                 ? qMin(1.0, achieved / static_cast<double>(target))
                                 : 0.0);
                snapshot.awards.append(award);
            };
            addAward(QStringLiteral("dxcc"), QStringLiteral("DXCC"),
                     query.value(0).toInt(), query.value(1).toInt(), 100,
                     QStringLiteral("Base DXCC: 100 distinct entities"));
            addAward(QStringLiteral("grid"), QStringLiteral("Maidenhead"),
                     query.value(2).toInt(), query.value(3).toInt(), 100,
                     QStringLiteral("Base grid target: 100 distinct 4-character grids"));
            addAward(QStringLiteral("waz"), QStringLiteral("WAZ"),
                     query.value(4).toInt(), query.value(5).toInt(), 40,
                     QStringLiteral("All 40 CQ zones"));
            addAward(QStringLiteral("was"), QStringLiteral("WAS"),
                     query.value(6).toInt(), query.value(7).toInt(), 50,
                     QStringLiteral("The 50 US states; territories are excluded"));
            addAward(QStringLiteral("itu"), QStringLiteral("ITU Zones"),
                     query.value(8).toInt(), query.value(9).toInt(), 90,
                     QStringLiteral("All 90 ITU zones"));
            addAward(QStringLiteral("us48"), QStringLiteral("US48"),
                     query.value(10).toInt(), query.value(11).toInt(), 48,
                     QStringLiteral("The contiguous 48 US states; Alaska and Hawaii are excluded"));
            addAward(QStringLiteral("wac"), QStringLiteral("WAC"),
                     query.value(12).toInt(), query.value(13).toInt(), 6,
                     QStringLiteral("Six populated continents; Antarctica is excluded"));
            addAward(QStringLiteral("pota"), QStringLiteral("POTA"),
                     query.value(14).toInt(), query.value(15).toInt(), 100,
                     QStringLiteral("Base POTA scorecard: 100 distinct park references"));
            addAward(QStringLiteral("iota"), QStringLiteral("IOTA"),
                     query.value(16).toInt(), query.value(17).toInt(), 100,
                     QStringLiteral("Base IOTA scorecard: 100 distinct island groups"));
            addAward(QStringLiteral("wpx"), QStringLiteral("WPX"),
                     query.value(18).toInt(), query.value(19).toInt(), 100,
                     QStringLiteral("Base WPX scorecard: 100 distinct prefixes"));

            // The external catalog is intentionally queried only for the
            // selected award. This still keeps refreshes bounded, while the
            // rows below let prefix/suffix equivalence rules be applied before
            // an entity is counted.
            if (ExternalAwardDefinition const* external =
                    externalAwardForLabel(options.activeAwardProgram)) {
                if (!externalAwardEntityExpression(*external).isEmpty()) {
                    QSqlQuery externalQuery(db);
                    externalQuery.prepare(QStringLiteral(
                        "SELECT call, grid4, band, mode, dxcc, cq_zone, state, continent,"
                        " county, iota, confirmed FROM map_qso WHERE 1=1")
                                              + awardQsoFilter
                                              + externalAwardScopeFilter(*external));
                    bindCommon(externalQuery, false);
                    if (externalQuery.exec()) {
                        QSet<QString> workedEntities;
                        QSet<QString> confirmedEntities;
                        while (externalQuery.next()) {
                            QString const entity = externalAwardSpotEntity(
                                *external,
                                externalQuery.value(0).toString(),
                                externalQuery.value(2).toString(),
                                externalQuery.value(1).toString(),
                                externalQuery.value(4).toString(),
                                externalQuery.value(5).toInt(),
                                externalQuery.value(6).toString(),
                                externalQuery.value(7).toString(),
                                externalQuery.value(8).toString(),
                                externalQuery.value(9).toString());
                            if (entity.isEmpty()
                                || !externalAwardMatchesSpot(
                                    *external, externalQuery.value(2).toString(),
                                    externalQuery.value(3).toString())) {
                                continue;
                            }
                            workedEntities.insert(entity);
                            if (externalQuery.value(10).toBool()) {
                                confirmedEntities.insert(entity);
                            }
                        }
                        addAward(external->id, external->label,
                                 workedEntities.size(), confirmedEntities.size(),
                                 external->target,
                                 external->tooltip.isEmpty()
                                     ? QStringLiteral("%1 rule: %2")
                                           .arg(external->sponsor, external->type)
                                     : external->tooltip);
                    }
                }
            }
        }
    }

    {
        QSqlQuery query(db);
        if (query.exec(QStringLiteral(
                "SELECT alert_type, call, grid, dxcc, message, created_ms, is_read"
                " FROM map_alert ORDER BY created_ms DESC LIMIT 50"))) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("type"), query.value(0).toString());
                row.insert(QStringLiteral("call"), query.value(1).toString());
                row.insert(QStringLiteral("grid"), query.value(2).toString());
                row.insert(QStringLiteral("dxcc"), query.value(3).toString());
                row.insert(QStringLiteral("message"), query.value(4).toString());
                row.insert(QStringLiteral("createdMs"), query.value(5).toLongLong());
                row.insert(QStringLiteral("read"), query.value(6).toBool());
                snapshot.alerts.append(row);
                if (!query.value(6).toBool()) ++snapshot.unreadAlertCount;
            }
        }
    }
    return snapshot;
}

MapIntelligenceService::GridDetails
MapIntelligenceService::queryGridDetails(const QString& databasePath,
                                         const QString& grid)
{
    GridDetails details;
    QString const fullGrid = normalizedGrid(grid);
    QString const normalized = fullGrid.left(fullGrid.size() >= 6 ? 6 : 4);
    QString const gridColumn =
        normalized.size() == 6 ? QStringLiteral("grid6") : QStringLiteral("grid4");
    details.summary.insert(QStringLiteral("grid"), normalized);
    if (normalized.isEmpty()) {
        details.error = QStringLiteral("Invalid Maidenhead grid");
        return details;
    }

    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, &details.error)) {
        return details;
    }
    QSqlDatabase& db = connection->database();
    int workedCount = 0;
    int confirmedCount = 0;
    int historicalCallCount = 0;
    int activeCount = 0;
    int pskCount = 0;
    int liveCallCount = 0;

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(confirmed), 0),"
            " COUNT(DISTINCT upper(call))"
            " FROM map_qso WHERE upper(%1)=upper(:grid)").arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        if (query.exec() && query.next()) {
            workedCount = query.value(0).toInt();
            confirmedCount = query.value(1).toInt();
            historicalCallCount = query.value(2).toInt();
        } else {
            details.error = query.lastError().text();
        }
    }

    qint64 const liveCutoff =
        QDateTime::currentMSecsSinceEpoch() - kLiveRetentionMs;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT COUNT(DISTINCT upper(call)),"
            " COUNT(DISTINCT CASE WHEN lower(source)='psk' THEN upper(call) END),"
            " COUNT(DISTINCT upper(call))"
            " FROM map_spot"
            " WHERE upper(%1)=upper(:grid) AND observed_ms>=:cutoff")
                          .arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        query.bindValue(QStringLiteral(":cutoff"), liveCutoff);
        if (query.exec() && query.next()) {
            activeCount = query.value(0).toInt();
            pskCount = query.value(1).toInt();
            liveCallCount = query.value(2).toInt();
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }

    details.summary.insert(QStringLiteral("workedCount"), workedCount);
    details.summary.insert(QStringLiteral("confirmedCount"), confirmedCount);
    details.summary.insert(QStringLiteral("historicalCallCount"), historicalCallCount);
    details.summary.insert(QStringLiteral("activeCount"), activeCount);
    details.summary.insert(QStringLiteral("pskCount"), pskCount);
    details.summary.insert(QStringLiteral("liveCallCount"), liveCallCount);
    details.summary.insert(QStringLiteral("worked"), workedCount > 0);
    details.summary.insert(QStringLiteral("confirmed"), confirmedCount > 0);
    details.summary.insert(QStringLiteral("active"), activeCount > 0);
    details.summary.insert(QStringLiteral("missing"), workedCount == 0 && activeCount > 0);
    details.summary.insert(QStringLiteral("psk"), pskCount > 0);

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT call, grid, band, mode, message, observed_utc, observed_ms,"
            " frequency_hz, snr, source, hits, dxcc, continent, state, is_cq,"
            " target_call, distance_km, activity_type"
            " FROM map_spot"
            " WHERE id IN ("
            "   SELECT MAX(id) FROM map_spot"
            "   WHERE upper(%1)=upper(:grid) AND observed_ms>=:cutoff"
            "   GROUP BY upper(call)"
            " )"
            " ORDER BY observed_ms DESC LIMIT 100").arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        query.bindValue(QStringLiteral(":cutoff"), liveCutoff);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("message"), query.value(4).toString());
                row.insert(QStringLiteral("observedUtc"), query.value(5).toString());
                row.insert(QStringLiteral("observedMs"), query.value(6).toLongLong());
                row.insert(QStringLiteral("frequencyHz"), query.value(7).toLongLong());
                row.insert(QStringLiteral("snr"), query.value(8).toInt());
                row.insert(QStringLiteral("source"), query.value(9).toString());
                row.insert(QStringLiteral("hits"), query.value(10).toInt());
                row.insert(QStringLiteral("dxcc"), query.value(11).toString());
                row.insert(QStringLiteral("continent"), query.value(12).toString());
                row.insert(QStringLiteral("state"), query.value(13).toString());
                row.insert(QStringLiteral("isCq"), query.value(14).toBool());
                row.insert(QStringLiteral("targetCall"), query.value(15).toString());
                row.insert(QStringLiteral("distanceKm"), query.value(16).toDouble());
                row.insert(QStringLiteral("activityType"), query.value(17).toString());
                QString const source = query.value(9).toString().trimmed().toLower();
                row.insert(QStringLiteral("gridEvidence"),
                           (source == QStringLiteral("psk") || source == QStringLiteral("oams"))
                               ? QStringLiteral("External spot locator")
                               : QStringLiteral("TX locator in decoded message"));
                details.live.append(row);
            }
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT call, grid, band, mode, qso_date, time_on, frequency_mhz,"
            " confirmed, source, dxcc, continent, state, qso_epoch"
            " FROM map_qso WHERE upper(%1)=upper(:grid)"
            " ORDER BY qso_epoch DESC, qso_date DESC, time_on DESC LIMIT 100")
                          .arg(gridColumn));
        query.bindValue(QStringLiteral(":grid"), normalized);
        if (query.exec()) {
            while (query.next()) {
                QVariantMap row;
                row.insert(QStringLiteral("call"), query.value(0).toString());
                row.insert(QStringLiteral("grid"), query.value(1).toString());
                row.insert(QStringLiteral("band"), query.value(2).toString());
                row.insert(QStringLiteral("mode"), query.value(3).toString());
                row.insert(QStringLiteral("qsoDate"), query.value(4).toString());
                row.insert(QStringLiteral("timeOn"), query.value(5).toString());
                row.insert(QStringLiteral("frequencyMhz"), query.value(6).toDouble());
                row.insert(QStringLiteral("confirmed"), query.value(7).toBool());
                row.insert(QStringLiteral("source"), query.value(8).toString());
                row.insert(QStringLiteral("dxcc"), query.value(9).toString());
                row.insert(QStringLiteral("continent"), query.value(10).toString());
                row.insert(QStringLiteral("state"), query.value(11).toString());
                row.insert(QStringLiteral("qsoEpoch"), query.value(12).toLongLong());
                details.qsos.append(row);
            }
        } else if (details.error.isEmpty()) {
            details.error = query.lastError().text();
        }
    }
    return details;
}

bool MapIntelligenceService::importAdifIntoDatabase(const QString& databasePath,
                                                    const QString& sourcePath,
                                                    const QByteArray& data,
                                                    const QString& fingerprint,
                                                    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT value FROM map_meta WHERE key = 'adif_fingerprint'"));
        if (query.exec() && query.next() && query.value(0).toString() == fingerprint) {
            return true;
        }
    }

    QList<QsoRecord> const records = parseAdif(data);
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (!execSql(db, QStringLiteral("DELETE FROM map_qso"), error)) {
        db.rollback();
        return false;
    }

    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO map_qso"
            " (source_key, call, grid, grid4, grid6, band, mode, qso_date, time_on,"
            " frequency_mhz, confirmed, qso_epoch, source, dxcc, continent,"
            " cq_zone, itu_zone, state, county, lotw_confirmed, eqsl_confirmed, oqrs,"
            " pota_ref, iota, wpx)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :band, :mode, :date, :time,"
            " :freq, :confirmed, :epoch, :source, :dxcc, :continent,"
            " :cq_zone, :itu_zone, :state, :county, :lotw_confirmed, :eqsl_confirmed, :oqrs,"
            " :pota_ref, :iota, :wpx)"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    for (QsoRecord const& record : records) {
        insert.bindValue(QStringLiteral(":key"), record.sourceKey);
        insert.bindValue(QStringLiteral(":call"), record.call);
        insert.bindValue(QStringLiteral(":grid"), record.grid);
        insert.bindValue(QStringLiteral(":grid4"), record.grid4);
        insert.bindValue(QStringLiteral(":grid6"), record.grid6);
        insert.bindValue(QStringLiteral(":band"), record.band);
        insert.bindValue(QStringLiteral(":mode"), record.mode);
        insert.bindValue(QStringLiteral(":date"), record.qsoDate);
        insert.bindValue(QStringLiteral(":time"), record.timeOn);
        insert.bindValue(QStringLiteral(":freq"), record.frequencyMhz);
        insert.bindValue(QStringLiteral(":confirmed"), record.confirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":epoch"), record.qsoEpoch);
        insert.bindValue(QStringLiteral(":source"), record.source);
        insert.bindValue(QStringLiteral(":dxcc"), record.dxcc);
        insert.bindValue(QStringLiteral(":continent"), record.continent);
        insert.bindValue(QStringLiteral(":cq_zone"), record.cqZone);
        insert.bindValue(QStringLiteral(":itu_zone"), record.ituZone);
        insert.bindValue(QStringLiteral(":state"), record.state);
        insert.bindValue(QStringLiteral(":county"), record.county);
        insert.bindValue(QStringLiteral(":lotw_confirmed"), record.lotwConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":eqsl_confirmed"), record.eqslConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":oqrs"), record.oqrs ? 1 : 0);
        insert.bindValue(QStringLiteral(":pota_ref"), record.potaReference);
        insert.bindValue(QStringLiteral(":iota"), record.iotaReference);
        insert.bindValue(QStringLiteral(":wpx"), record.wpxPrefix);
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.rollback();
            return false;
        }
    }

    QSqlQuery meta(db);
    meta.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO map_meta(key, value) VALUES(:key, :value)"));
    auto writeMeta = [&meta](QString const& key, QString const& value) {
        meta.bindValue(QStringLiteral(":key"), key);
        meta.bindValue(QStringLiteral(":value"), value);
        return meta.exec();
    };
    if (!writeMeta(QStringLiteral("adif_fingerprint"), fingerprint)
        || !writeMeta(QStringLiteral("adif_source_path"), sourcePath)) {
        if (error) *error = meta.lastError().text();
        db.rollback();
        return false;
    }
    return db.commit();
}

bool MapIntelligenceService::appendQsoRecords(const QString& databasePath,
                                              const QList<QsoRecord>& records,
                                              QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO map_qso"
            " (source_key, call, grid, grid4, grid6, band, mode, qso_date, time_on,"
            " frequency_mhz, confirmed, qso_epoch, source, dxcc, continent,"
            " cq_zone, itu_zone, state, county, lotw_confirmed, eqsl_confirmed, oqrs,"
            " pota_ref, iota, wpx)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :band, :mode, :date, :time,"
            " :freq, :confirmed, :epoch, :source, :dxcc, :continent,"
            " :cq_zone, :itu_zone, :state, :county, :lotw_confirmed, :eqsl_confirmed, :oqrs,"
            " :pota_ref, :iota, :wpx)"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    for (QsoRecord const& record : records) {
        insert.bindValue(QStringLiteral(":key"), record.sourceKey);
        insert.bindValue(QStringLiteral(":call"), record.call);
        insert.bindValue(QStringLiteral(":grid"), record.grid);
        insert.bindValue(QStringLiteral(":grid4"), record.grid4);
        insert.bindValue(QStringLiteral(":grid6"), record.grid6);
        insert.bindValue(QStringLiteral(":band"), record.band);
        insert.bindValue(QStringLiteral(":mode"), record.mode);
        insert.bindValue(QStringLiteral(":date"), record.qsoDate);
        insert.bindValue(QStringLiteral(":time"), record.timeOn);
        insert.bindValue(QStringLiteral(":freq"), record.frequencyMhz);
        insert.bindValue(QStringLiteral(":confirmed"), record.confirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":epoch"), record.qsoEpoch);
        insert.bindValue(QStringLiteral(":source"), record.source);
        insert.bindValue(QStringLiteral(":dxcc"), record.dxcc);
        insert.bindValue(QStringLiteral(":continent"), record.continent);
        insert.bindValue(QStringLiteral(":cq_zone"), record.cqZone);
        insert.bindValue(QStringLiteral(":itu_zone"), record.ituZone);
        insert.bindValue(QStringLiteral(":state"), record.state);
        insert.bindValue(QStringLiteral(":county"), record.county);
        insert.bindValue(QStringLiteral(":lotw_confirmed"), record.lotwConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":eqsl_confirmed"), record.eqslConfirmed ? 1 : 0);
        insert.bindValue(QStringLiteral(":oqrs"), record.oqrs ? 1 : 0);
        insert.bindValue(QStringLiteral(":pota_ref"), record.potaReference);
        insert.bindValue(QStringLiteral(":iota"), record.iotaReference);
        insert.bindValue(QStringLiteral(":wpx"), record.wpxPrefix);
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.rollback();
            return false;
        }
    }
    return db.commit();
}

bool MapIntelligenceService::appendLiveSpots(const QString& databasePath,
                                             const QList<LiveSpot>& spots,
                                             const AlertRules& rules,
                                             QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO map_spot"
            " (unique_key, call, grid, grid4, grid6, band, mode, message, observed_utc,"
            " observed_ms, frequency_hz, snr, source, dxcc, continent, cq_zone,"
            " itu_zone, state, is_cq, target_call, distance_km, activity_type,"
            " receiver_call, receiver_grid, provider, first_observed_ms, last_observed_ms,"
            " correlation_count)"
            " VALUES (:key, :call, :grid, :grid4, :grid6, :band, :mode, :message, :utc,"
            " :ms, :freq, :snr, :source, :dxcc, :continent, :cq_zone,"
            " :itu_zone, :state, :is_cq, :target_call, :distance_km, :activity_type,"
            " :receiver_call, :receiver_grid, :provider, :first_observed_ms, :last_observed_ms,"
            " :correlation_count)"
            " ON CONFLICT(unique_key) DO UPDATE SET"
            " observed_utc=excluded.observed_utc,"
            " observed_ms=excluded.observed_ms,"
            " last_observed_ms=excluded.last_observed_ms,"
            " snr=excluded.snr,"
            " receiver_call=excluded.receiver_call,"
            " receiver_grid=excluded.receiver_grid,"
            " provider=excluded.provider,"
            " correlation_count=MAX(map_spot.correlation_count, excluded.correlation_count),"
            " activity_type=excluded.activity_type,"
            " hits=map_spot.hits+1"))) {
        if (error) *error = insert.lastError().text();
        db.rollback();
        return false;
    }
    QSqlQuery correlated(db);
    correlated.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT lower(source)) FROM map_spot"
        " WHERE upper(call)=upper(:call)"
        " AND (:grid='' OR upper(grid4)=upper(:grid))"
        " AND lower(source)<>lower(:source)"
        " AND last_observed_ms>=:cutoff"));
    QSqlQuery event(db);
    if (!event.prepare(QStringLiteral(
            "INSERT INTO map_spot_event"
            " (spot_key, call, grid, receiver_call, receiver_grid, band, mode, source, provider,"
            " observed_ms, frequency_hz, snr, correlation, activity_type)"
            " VALUES (:spot_key, :call, :grid, :receiver_call, :receiver_grid, :band, :mode,"
            " :source, :provider, :observed_ms, :frequency_hz, :snr, :correlation, :activity_type)"))) {
        if (error) *error = event.lastError().text();
        db.rollback();
        return false;
    }
    for (LiveSpot const& spot : spots) {
        correlated.bindValue(QStringLiteral(":call"), spot.call);
        correlated.bindValue(QStringLiteral(":grid"), spot.grid4);
        correlated.bindValue(QStringLiteral(":source"), spot.source);
        correlated.bindValue(QStringLiteral(":cutoff"), spot.observedMs - 5 * 60 * 1000LL);
        int correlationCount = 0;
        if (correlated.exec() && correlated.next()) {
            correlationCount = correlated.value(0).toInt();
        }
        insert.bindValue(QStringLiteral(":key"), spot.uniqueKey);
        insert.bindValue(QStringLiteral(":call"), spot.call);
        insert.bindValue(QStringLiteral(":grid"), spot.grid);
        insert.bindValue(QStringLiteral(":grid4"), spot.grid4);
        insert.bindValue(QStringLiteral(":grid6"), spot.grid6);
        insert.bindValue(QStringLiteral(":band"), spot.band);
        insert.bindValue(QStringLiteral(":mode"), spot.mode);
        insert.bindValue(QStringLiteral(":message"), spot.message);
        insert.bindValue(QStringLiteral(":utc"), spot.observedUtc);
        insert.bindValue(QStringLiteral(":ms"), spot.observedMs);
        insert.bindValue(QStringLiteral(":freq"), spot.frequencyHz);
        insert.bindValue(QStringLiteral(":snr"), spot.snr);
        insert.bindValue(QStringLiteral(":source"), spot.source);
        insert.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
        insert.bindValue(QStringLiteral(":continent"), spot.continent);
        insert.bindValue(QStringLiteral(":cq_zone"), spot.cqZone);
        insert.bindValue(QStringLiteral(":itu_zone"), spot.ituZone);
        insert.bindValue(QStringLiteral(":state"), spot.state);
        insert.bindValue(QStringLiteral(":is_cq"), spot.isCq ? 1 : 0);
        insert.bindValue(QStringLiteral(":target_call"), spot.targetCall);
        insert.bindValue(QStringLiteral(":distance_km"), spot.distanceKm);
        insert.bindValue(QStringLiteral(":activity_type"), spot.activityType);
        insert.bindValue(QStringLiteral(":receiver_call"), spot.receiverCall);
        insert.bindValue(QStringLiteral(":receiver_grid"), spot.receiverGrid);
        insert.bindValue(QStringLiteral(":provider"), spot.provider);
        insert.bindValue(QStringLiteral(":first_observed_ms"), spot.observedMs);
        insert.bindValue(QStringLiteral(":last_observed_ms"), spot.observedMs);
        insert.bindValue(QStringLiteral(":correlation_count"), correlationCount);
        if (!insert.exec()) {
            if (error) *error = insert.lastError().text();
            db.rollback();
            return false;
        }
        event.bindValue(QStringLiteral(":spot_key"), spot.uniqueKey);
        event.bindValue(QStringLiteral(":call"), spot.call);
        event.bindValue(QStringLiteral(":grid"), spot.grid);
        event.bindValue(QStringLiteral(":receiver_call"), spot.receiverCall);
        event.bindValue(QStringLiteral(":receiver_grid"), spot.receiverGrid);
        event.bindValue(QStringLiteral(":band"), spot.band);
        event.bindValue(QStringLiteral(":mode"), spot.mode);
        event.bindValue(QStringLiteral(":source"), spot.source);
        event.bindValue(QStringLiteral(":provider"), spot.provider);
        event.bindValue(QStringLiteral(":observed_ms"), spot.observedMs);
        event.bindValue(QStringLiteral(":frequency_hz"), spot.frequencyHz);
        event.bindValue(QStringLiteral(":snr"), spot.snr);
        event.bindValue(QStringLiteral(":correlation"), correlationCount);
        event.bindValue(QStringLiteral(":activity_type"), spot.activityType);
        if (!event.exec()) {
            if (error) *error = event.lastError().text();
            db.rollback();
            return false;
        }

        auto addAlert = [&db, &spot](QString const& type, QString const& text) {
            QString const day = QDateTime::fromMSecsSinceEpoch(
                spot.observedMs, QTimeZone::UTC).date().toString(Qt::ISODate);
            QSqlQuery alert(db);
            alert.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO map_alert"
                " (alert_key, alert_type, call, grid, dxcc, message, created_ms, is_read)"
                " VALUES (:key, :type, :call, :grid, :dxcc, :message, :created, 0)"));
            alert.bindValue(QStringLiteral(":key"),
                            digestKey({type, spot.call, spot.grid4, spot.dxcc, day}));
            alert.bindValue(QStringLiteral(":type"), type);
            alert.bindValue(QStringLiteral(":call"), spot.call);
            alert.bindValue(QStringLiteral(":grid"), spot.grid);
            alert.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
            alert.bindValue(QStringLiteral(":message"), text);
            alert.bindValue(QStringLiteral(":created"), spot.observedMs);
            alert.exec();
        };

        if (rules.newGridEnabled && !spot.grid4.isEmpty()) {
            QSqlQuery knownGrid(db);
            knownGrid.prepare(QStringLiteral(
                "SELECT 1 FROM map_qso WHERE grid4=:grid LIMIT 1"));
            knownGrid.bindValue(QStringLiteral(":grid"), spot.grid4);
            if (knownGrid.exec() && !knownGrid.next()) {
                addAlert(QStringLiteral("new_grid"),
                         QStringLiteral("%1 active from new grid %2")
                             .arg(spot.call, spot.grid4));
            }
        }
        if (rules.newDxccEnabled && !spot.dxcc.isEmpty()) {
            QSqlQuery knownDxcc(db);
            knownDxcc.prepare(QStringLiteral(
                "SELECT 1 FROM map_qso WHERE lower(dxcc)=lower(:dxcc) LIMIT 1"));
            knownDxcc.bindValue(QStringLiteral(":dxcc"), spot.dxcc);
            if (knownDxcc.exec() && !knownDxcc.next()) {
                addAlert(QStringLiteral("new_dxcc"),
                         QStringLiteral("%1 active from new DXCC %2")
                             .arg(spot.call, spot.dxcc));
            }
        }
        if (rules.cqEnabled && spot.isCq) {
            addAlert(QStringLiteral("cq"),
                     QStringLiteral("CQ from %1 %2")
                         .arg(spot.call, spot.grid4));
        }
        if (!rules.callPattern.isEmpty()) {
            QRegularExpression const expression(
                QRegularExpression::wildcardToRegularExpression(
                    rules.callPattern,
                    QRegularExpression::UnanchoredWildcardConversion),
                QRegularExpression::CaseInsensitiveOption);
            if (expression.isValid()
                && (expression.match(spot.call).hasMatch()
                    || expression.match(spot.message).hasMatch())) {
                addAlert(QStringLiteral("call_watch"),
                         QStringLiteral("%1 matched alert pattern %2")
                             .arg(spot.call, rules.callPattern));
            }
        }
    }
    QSqlQuery prune(db);
    prune.prepare(QStringLiteral("DELETE FROM map_spot WHERE observed_ms < :cutoff"));
    prune.bindValue(QStringLiteral(":cutoff"),
                    QDateTime::currentMSecsSinceEpoch() - kLiveRetentionMs);
    prune.exec();
    QSqlQuery pruneEvents(db);
    pruneEvents.prepare(QStringLiteral("DELETE FROM map_spot_event WHERE observed_ms < :cutoff"));
    pruneEvents.bindValue(QStringLiteral(":cutoff"),
                          QDateTime::currentMSecsSinceEpoch() - kSpotEventRetentionMs);
    pruneEvents.exec();
    execSql(db, QStringLiteral(
        "DELETE FROM map_spot WHERE id NOT IN"
        " (SELECT id FROM map_spot ORDER BY observed_ms DESC LIMIT 5000)"), nullptr);
    execSql(db, QStringLiteral(
        "DELETE FROM map_spot_event WHERE id NOT IN"
        " (SELECT id FROM map_spot_event ORDER BY observed_ms DESC LIMIT 100000)"), nullptr);
    execSql(db, QStringLiteral(
        "DELETE FROM map_alert WHERE id NOT IN"
        " (SELECT id FROM map_alert ORDER BY created_ms DESC LIMIT 500)"), nullptr);
    return db.commit();
}

bool MapIntelligenceService::clearLiveSpotRows(const QString& databasePath,
                                               QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    return execSql(connection->database(), QStringLiteral("DELETE FROM map_spot"), error);
}

bool MapIntelligenceService::clearPskHeardByRows(const QString& databasePath,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) {
        return false;
    }
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    // The HTTP endpoint returns a replacement snapshot for this station. Keep
    // MQTT events, which are an independent continuous feed, untouched.
    QString const predicate = QStringLiteral(
        "lower(source)='psk'"
        " AND lower(COALESCE(provider, '')) <> 'psk reporter mqtt'");
    if (!execSql(db, QStringLiteral("DELETE FROM map_spot WHERE ") + predicate, error)
        || !execSql(db, QStringLiteral("DELETE FROM map_spot_event WHERE ") + predicate,
                    error)) {
        db.rollback();
        return false;
    }
    return db.commit();
}

bool MapIntelligenceService::clearAlertRows(const QString& databasePath,
                                            QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    return execSql(connection->database(), QStringLiteral("DELETE FROM map_alert"), error);
}

bool MapIntelligenceService::markAlertRowsRead(const QString& databasePath,
                                               QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    return execSql(connection->database(),
                   QStringLiteral("UPDATE map_alert SET is_read=1 WHERE is_read=0"),
                   error);
}

bool MapIntelligenceService::updateRosterPreference(const QString& databasePath,
                                                    const QString& call,
                                                    bool watched,
                                                    bool ignored,
                                                    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!watched && !ignored) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral(
            "DELETE FROM map_roster_preference WHERE call=:call"));
        remove.bindValue(QStringLiteral(":call"), call);
        if (!remove.exec()) {
            if (error) *error = remove.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO map_roster_preference(call, watched, ignored, updated_ms)"
        " VALUES(:call, :watched, :ignored, :updated_ms)"
        " ON CONFLICT(call) DO UPDATE SET"
        " watched=excluded.watched,"
        " ignored=excluded.ignored,"
        " updated_ms=excluded.updated_ms"));
    upsert.bindValue(QStringLiteral(":call"), call);
    upsert.bindValue(QStringLiteral(":watched"), watched ? 1 : 0);
    upsert.bindValue(QStringLiteral(":ignored"), ignored ? 1 : 0);
    upsert.bindValue(QStringLiteral(":updated_ms"), QDateTime::currentMSecsSinceEpoch());
    if (!upsert.exec()) {
        if (error) *error = upsert.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::updateRosterIgnore(const QString& databasePath,
                                                const QString& type,
                                                const QString& value,
                                                bool ignored,
                                                QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!ignored) {
        QSqlQuery remove(db);
        remove.prepare(QStringLiteral(
            "DELETE FROM map_roster_ignore"
            " WHERE upper(ignore_type)=upper(:type)"
            " AND upper(ignore_value)=upper(:value)"));
        remove.bindValue(QStringLiteral(":type"), type);
        remove.bindValue(QStringLiteral(":value"), value);
        if (!remove.exec()) {
            if (error) *error = remove.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery upsert(db);
    upsert.prepare(QStringLiteral(
        "INSERT INTO map_roster_ignore(ignore_type, ignore_value, updated_ms)"
        " VALUES(upper(:type), :value, :updated_ms)"
        " ON CONFLICT(ignore_type, ignore_value) DO UPDATE SET"
        " updated_ms=excluded.updated_ms"));
    upsert.bindValue(QStringLiteral(":type"), type);
    upsert.bindValue(QStringLiteral(":value"), value);
    upsert.bindValue(QStringLiteral(":updated_ms"),
                     QDateTime::currentMSecsSinceEpoch());
    if (!upsert.exec()) {
        if (error) *error = upsert.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::removeRosterPreferenceRow(
    const QString& databasePath,
    const QString& type,
    const QString& value,
    QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    bool success = true;
    QSqlQuery query(db);
    if (type == QStringLiteral("WATCH")
        || type == QStringLiteral("CALL")) {
        QString const column = type == QStringLiteral("WATCH")
            ? QStringLiteral("watched") : QStringLiteral("ignored");
        query.prepare(QStringLiteral(
            "UPDATE map_roster_preference SET %1=0, updated_ms=:updated_ms"
            " WHERE upper(call)=upper(:value)").arg(column));
        query.bindValue(QStringLiteral(":updated_ms"),
                        QDateTime::currentMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":value"), value);
        success = query.exec();
        if (success) {
            success = execSql(
                db,
                QStringLiteral(
                    "DELETE FROM map_roster_preference"
                    " WHERE watched=0 AND ignored=0"),
                error);
        }
    } else {
        query.prepare(QStringLiteral(
            "DELETE FROM map_roster_ignore"
            " WHERE upper(ignore_type)=upper(:type)"
            " AND upper(ignore_value)=upper(:value)"));
        query.bindValue(QStringLiteral(":type"), type);
        query.bindValue(QStringLiteral(":value"), value);
        success = query.exec();
    }

    if (!success) {
        if (error && error->isEmpty()) *error = query.lastError().text();
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::clearRosterPreferenceRows(const QString& databasePath,
                                                       QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlDatabase& db = connection->database();
    if (!db.transaction()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    if (!execSql(db, QStringLiteral("DELETE FROM map_roster_preference"), error)
        || !execSql(db, QStringLiteral("DELETE FROM map_roster_ignore"), error)) {
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        if (error) *error = db.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::updateRosterRuleRow(const QString& databasePath,
                                                 const QString& type,
                                                 const QString& value,
                                                 const QString& action,
                                                 const QString& band,
                                                 const QString& mode,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "INSERT INTO map_roster_rule(rule_type, rule_value, rule_action, band, mode, enabled, updated_ms)"
        " VALUES(upper(:type), upper(:value), upper(:action), lower(:band), upper(:mode), 1, :updated)"
        " ON CONFLICT(rule_type, rule_value, band, mode) DO UPDATE SET"
        " rule_action=excluded.rule_action, enabled=1, updated_ms=excluded.updated_ms"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":value"), value);
    query.bindValue(QStringLiteral(":action"), action);
    query.bindValue(QStringLiteral(":band"), band.trimmed());
    query.bindValue(QStringLiteral(":mode"), mode.trimmed());
    query.bindValue(QStringLiteral(":updated"), QDateTime::currentMSecsSinceEpoch());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool MapIntelligenceService::removeRosterRuleRow(const QString& databasePath,
                                                 const QString& type,
                                                 const QString& value,
                                                 const QString& band,
                                                 const QString& mode,
                                                 QString* error)
{
    std::unique_ptr<ScopedSqliteConnection> connection;
    if (!openMapDatabase(databasePath, &connection, error)) return false;
    QSqlQuery query(connection->database());
    query.prepare(QStringLiteral(
        "DELETE FROM map_roster_rule WHERE upper(rule_type)=upper(:type)"
        " AND upper(rule_value)=upper(:value) AND lower(band)=lower(:band)"
        " AND upper(mode)=upper(:mode)"));
    query.bindValue(QStringLiteral(":type"), type);
    query.bindValue(QStringLiteral(":value"), value);
    query.bindValue(QStringLiteral(":band"), band.trimmed());
    query.bindValue(QStringLiteral(":mode"), mode.trimmed());
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

void MapIntelligenceService::queueSnapshotQuery(quint64 generation)
{
    QString const database = m_databasePath;
    QueryOptions const options {
        m_bandFilter, m_modeFilter, m_periodFilter, m_continentFilter,
        m_dxccFilter, m_sourceFilter, m_rosterSort, m_rosterStatusFilter,
        m_rosterHuntScope, m_activeAwardProgram, m_awardGoal,
        m_rosterRetentionMinutes, m_gridPrecision, m_liveDecayMinutes,
        m_cqOnly, m_rosterSortDescending, m_rosterCqOnly,
        m_splitGridEnabled, m_rosterTextFilter, m_rosterTextMode,
        pskLayerEnabled(), m_pskDisplayMode, m_pskOpacityPercent / 100.0,
        m_spotAgeFilter, m_spotCorrelationFilter
    };
    QPointer<MapIntelligenceService> guard(this);
    m_workerPool.start(QRunnable::create([guard, database, options, generation] {
        Snapshot snapshot = queryDatabase(database, options);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard.data(),
            [guard, generation, snapshot = std::move(snapshot)]() mutable {
                if (guard) {
                    guard->applySnapshot(generation, std::move(snapshot));
                }
            }, Qt::QueuedConnection);
    }));
}

void MapIntelligenceService::applySnapshot(quint64 generation, Snapshot snapshot)
{
    if (generation != m_queryGeneration.load()) {
        return;
    }
    if (!snapshot.error.isEmpty()) {
        qWarning().noquote() << "[MAPINT] SQLite query failed:" << snapshot.error;
    }

    m_rawCoverage = std::move(snapshot.coverage);
    m_roster = std::move(snapshot.roster);
    m_rosterPreferences = std::move(snapshot.rosterPreferences);
    m_awards = std::move(snapshot.awards);
    m_alerts = std::move(snapshot.alerts);
    m_spotHeatmap = std::move(snapshot.spotHeatmap);
    m_spotTimeline = std::move(snapshot.spotTimeline);
    m_spotPaths = std::move(snapshot.spotPaths);
    m_rosterRules = std::move(snapshot.rosterRules);
    m_statistics = std::move(snapshot.statistics);
    m_availableBands = std::move(snapshot.bands);
    m_availableModes = std::move(snapshot.modes);
    m_availableContinents = std::move(snapshot.continents);
    m_availableDxcc = std::move(snapshot.dxcc);
    m_availableSources = std::move(snapshot.sources);
    m_qsoCount = snapshot.qsoCount;
    m_workedGridCount = snapshot.workedGridCount;
    m_confirmedGridCount = snapshot.confirmedGridCount;
    m_activeGridCount = snapshot.activeGridCount;
    m_missingGridCount = snapshot.missingGridCount;
    m_liveSpotCount = snapshot.liveSpotCount;
    m_rosterWantedCount = snapshot.rosterWantedCount;
    m_rosterNewCount = snapshot.rosterNewCount;
    m_rosterUnconfirmedCount = snapshot.rosterUnconfirmedCount;
    m_unreadAlertCount = snapshot.unreadAlertCount;

    if (!m_availableBands.contains(m_bandFilter, Qt::CaseInsensitive)) {
        m_bandFilter = QStringLiteral("All");
        emit bandFilterChanged();
    }
    if (!m_availableModes.contains(m_modeFilter, Qt::CaseInsensitive)) {
        m_modeFilter = QStringLiteral("All");
        emit modeFilterChanged();
    }
    if (!m_availableContinents.contains(m_continentFilter, Qt::CaseInsensitive)) {
        m_continentFilter = QStringLiteral("All");
        emit continentFilterChanged();
    }
    if (!m_availableDxcc.contains(m_dxccFilter, Qt::CaseInsensitive)) {
        m_dxccFilter = QStringLiteral("All");
        emit dxccFilterChanged();
    }
    if (!m_availableSources.contains(m_sourceFilter, Qt::CaseInsensitive)) {
        m_sourceFilter = QStringLiteral("All");
        emit sourceFilterChanged();
    }

    m_layerModel->setCount(QStringLiteral("live"), m_liveSpotCount);
    m_layerModel->setCount(QStringLiteral("worked"), m_workedGridCount);
    m_layerModel->setCount(QStringLiteral("confirmed"), m_confirmedGridCount);
    m_layerModel->setCount(QStringLiteral("active"), m_activeGridCount);
    m_layerModel->setCount(QStringLiteral("missing"), m_missingGridCount);
    m_layerModel->setCount(QStringLiteral("psk"), snapshot.pskListenerCount);
    emit filtersChanged();
    emit rosterChanged();
    emit rosterPreferencesChanged();
    emit awardsChanged();
    emit alertsChanged();
    emit spotAnalyticsChanged();
    emit rosterRulesChanged();
    emit statisticsChanged();
    rebuildVisibleCoverage();

    qInfo().noquote()
        << QStringLiteral("[MAPINT] snapshot qso=%1 worked=%2 confirmed=%3 active=%4 missing=%5 live=%6 roster=%7 wanted=%8 alerts=%9 db=%10")
               .arg(m_qsoCount)
               .arg(m_workedGridCount)
               .arg(m_confirmedGridCount)
               .arg(m_activeGridCount)
               .arg(m_missingGridCount)
               .arg(m_liveSpotCount)
               .arg(m_roster.size())
               .arg(m_rosterWantedCount)
               .arg(m_unreadAlertCount)
               .arg(m_databasePath);
}

void MapIntelligenceService::applyGridDetails(quint64 generation,
                                              const QString& grid,
                                              GridDetails details)
{
    if (generation != m_gridDetailsGeneration.load()
        || m_selectedGrid.compare(grid, Qt::CaseInsensitive) != 0) {
        return;
    }
    if (!details.error.isEmpty()) {
        qWarning().noquote()
            << "[MAPINT] grid details query failed grid=" << grid
            << "error=" << details.error;
    }
    m_selectedGridSummary = std::move(details.summary);
    m_selectedGridLive = std::move(details.live);
    m_selectedGridQsos = std::move(details.qsos);
    setGridDetailsLoading(false);
    emit gridDetailsChanged();
}

void MapIntelligenceService::rebuildVisibleCoverage()
{
    QVariantList visible;
    bool const showWorked = workedLayerEnabled();
    bool const showConfirmed = confirmedLayerEnabled();
    // Coverage overlays are independent map layers. Requiring the generic
    // live-station layer here made Active/Missing/PSK counters non-zero while
    // rendering no cells when LIVE was toggled off.
    bool const showActive = activeLayerEnabled();
    bool const showMissing = missingLayerEnabled();
    bool const showPsk = pskLayerEnabled();
    visible.reserve(m_rawCoverage.size());
    for (QVariant const& value : std::as_const(m_rawCoverage)) {
        QVariantMap row = value.toMap();
        bool const confirmed = row.value(QStringLiteral("confirmed")).toBool();
        bool const worked = row.value(QStringLiteral("workedCount")).toInt() > 0;
        bool const active = row.value(QStringLiteral("active")).toBool();
        bool const missing = row.value(QStringLiteral("missing")).toBool();
        bool const psk = row.value(QStringLiteral("psk")).toBool();
        if ((!worked || !showWorked)
            && (!confirmed || !showConfirmed)
            && (!active || !showActive)
            && (!missing || !showMissing)
            && (!psk || !showPsk)) {
            continue;
        }
        row.insert(QStringLiteral("confirmed"), confirmed && showConfirmed);
        row.insert(QStringLiteral("worked"), worked && showWorked);
        row.insert(QStringLiteral("active"), active && showActive);
        row.insert(QStringLiteral("missing"), missing && showMissing);
        row.insert(QStringLiteral("psk"), psk && showPsk);
        visible.append(row);
    }
    m_coverageCells = std::move(visible);
    emit coverageChanged();
}

void MapIntelligenceService::setGridDetailsLoading(bool loading)
{
    if (m_gridDetailsLoading == loading) {
        return;
    }
    m_gridDetailsLoading = loading;
    emit gridDetailsLoadingChanged();
}

void MapIntelligenceService::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void MapIntelligenceService::saveSetting(const QString& key, const QVariant& value) const
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    settings.beginGroup(QStringLiteral("LiveMapLayers"));
    settings.setValue(key, value);
    settings.endGroup();
    settings.sync();
}
