#include "MapBaseMapService.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QLinearGradient>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QtMath>

#include <cmath>
#include <utility>

namespace {

constexpr int kNetworkTimeoutMs = 20000;
constexpr int kMaxPayloadBytes = 24 * 1024 * 1024;
constexpr int kOutputWidth = 2048;
constexpr int kOutputHeight = 1024;

QString settingsOrganization()
{
    return QStringLiteral("Decodium");
}

QString settingsApplication()
{
    return QStringLiteral("Decodium3");
}

} // namespace

MapBaseMapService::MapBaseMapService(QObject* parent, const QString& cachePath)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_localAtlas(loadLocalAtlas())
    , m_currentImage(m_localAtlas)
    , m_cachePath(cachePath.trimmed().isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                .absoluteFilePath(QStringLiteral("map-base"))
          : QFileInfo(cachePath).absoluteFilePath())
{
    QDir().mkpath(m_cachePath);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    m_provider = normalizeProvider(settings.value(
        QStringLiteral("Provider"), QStringLiteral("Decodium Atlas")).toString());
    m_offlineMode = settings.value(QStringLiteral("OfflineMode"), false).toBool();
    m_mapTilerApiKey = settings.value(QStringLiteral("MapTilerApiKey")).toString().trimmed();
    settings.endGroup();

    if (m_offlineMode) {
        setStatus(QStringLiteral("Offline: Decodium Atlas"));
    } else if (m_provider != QStringLiteral("Decodium Atlas")) {
        loadCachedOnlineImage();
        QMetaObject::invokeMethod(this, &MapBaseMapService::refresh,
                                  Qt::QueuedConnection);
    } else {
        setStatus(QStringLiteral("Decodium Atlas (local)"));
    }
}

QStringList MapBaseMapService::availableProviders() const
{
    return {
        QStringLiteral("Decodium Atlas"),
        QStringLiteral("NASA GIBS satellite"),
        QStringLiteral("MapTiler satellite")
    };
}

QString MapBaseMapService::attribution() const
{
    if (m_offlineMode) {
        return QStringLiteral("Decodium Atlas (local)");
    }
    if (m_provider == QStringLiteral("NASA GIBS satellite")) {
        return QStringLiteral("NASA EOSDIS GIBS");
    }
    if (m_provider == QStringLiteral("MapTiler satellite")) {
        return QStringLiteral("MapTiler / OpenStreetMap contributors");
    }
    return QStringLiteral("Decodium Atlas (local)");
}

QString MapBaseMapService::attributionUrl() const
{
    if (m_offlineMode) {
        return {};
    }
    if (m_provider == QStringLiteral("NASA GIBS satellite")) {
        return QStringLiteral("https://www.earthdata.nasa.gov/eosdis/science-system-description/eosdis-components/gibs");
    }
    if (m_provider == QStringLiteral("MapTiler satellite")) {
        return QStringLiteral("https://www.maptiler.com/copyright/");
    }
    return {};
}

void MapBaseMapService::setProvider(const QString& provider)
{
    QString const next = normalizeProvider(provider);
    if (m_provider == next) {
        return;
    }

    ++m_generation;
    m_provider = next;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("Provider"), m_provider);
    settings.endGroup();
    settings.sync();
    emit providerChanged();
    if (m_offlineMode) {
        applyImage(m_localAtlas, QStringLiteral("Offline: Decodium Atlas"));
    } else {
        refresh();
    }
}

void MapBaseMapService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }
    ++m_generation;
    m_offlineMode = offline;
    if (m_offlineMode) {
        for (QNetworkReply* reply : findChildren<QNetworkReply*>()) {
            reply->abort();
        }
        m_tiles.clear();
        m_tileExpected = 0;
        m_tileCompleted = 0;
        m_tileFailures = 0;
        m_currentImage = m_localAtlas;
        emit baseMapImageChanged();
        setLoading(false);
        setStatus(QStringLiteral("Offline: Decodium Atlas"));
    } else {
        setStatus(QStringLiteral("Online base map ready"));
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("OfflineMode"), m_offlineMode);
    settings.endGroup();
    settings.sync();
    emit offlineModeChanged();
    emit providerChanged();
    if (!m_offlineMode) {
        refresh();
    }
}

void MapBaseMapService::setMapTilerApiKey(const QString& apiKey)
{
    QString const key = apiKey.trimmed();
    if (m_mapTilerApiKey == key) {
        return;
    }
    m_mapTilerApiKey = key;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       settingsOrganization(), settingsApplication());
    settings.beginGroup(QStringLiteral("LiveMapBase"));
    settings.setValue(QStringLiteral("MapTilerApiKey"), m_mapTilerApiKey);
    settings.endGroup();
    settings.sync();
    emit mapTilerApiKeyChanged();
    if (!m_offlineMode && m_provider == QStringLiteral("MapTiler satellite")) {
        refresh();
    }
}

void MapBaseMapService::refresh()
{
    if (m_offlineMode || m_provider == QStringLiteral("Decodium Atlas")) {
        applyImage(m_localAtlas, m_offlineMode
            ? QStringLiteral("Offline: Decodium Atlas")
            : QStringLiteral("Decodium Atlas (local)"));
        return;
    }
    if (m_provider == QStringLiteral("NASA GIBS satellite")) {
        requestNasaGibs();
        return;
    }
    if (m_provider == QStringLiteral("MapTiler satellite")) {
        requestMapTilerTiles();
    }
}

QString MapBaseMapService::normalizeProvider(const QString& provider)
{
    QString const normalized = provider.trimmed();
    if (normalized.compare(QStringLiteral("NASA GIBS satellite"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("NASA GIBS satellite");
    }
    if (normalized.compare(QStringLiteral("MapTiler satellite"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("MapTiler satellite");
    }
    return QStringLiteral("Decodium Atlas");
}

QImage MapBaseMapService::loadLocalAtlas()
{
    QString const appDir = QCoreApplication::applicationDirPath();
    QString const cwd = QDir::currentPath();
    QStringList const candidates {
        QStringLiteral(":/earth_2048x1024.jpg"),
        QStringLiteral(":/artwork/maps/earth_2048x1024.jpg"),
        QDir(appDir).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/earth_2048x1024.jpg")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/wsjtx/maps/earth_2048x1024.jpg")),
        QDir(cwd).absoluteFilePath(QStringLiteral("artwork/maps/earth_2048x1024.jpg"))
    };
    for (QString const& path : candidates) {
        QImage const image(path);
        if (!image.isNull()) {
            return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
    }

    QImage image(kOutputWidth, kOutputHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(5, 24, 42));
    QPainter painter(&image);
    QLinearGradient gradient(QPointF(0, 0), QPointF(0, image.height()));
    gradient.setColorAt(0.0, QColor(9, 55, 91));
    gradient.setColorAt(1.0, QColor(3, 19, 34));
    painter.fillRect(image.rect(), gradient);
    painter.end();
    return image;
}

QImage MapBaseMapService::webMercatorToEquirectangular(const QImage& source,
                                                        const QSize& outputSize)
{
    if (source.isNull() || outputSize.isEmpty()) {
        return {};
    }
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QImage const input = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    constexpr double latitudeLimit = 85.0511287798066;
    double const pi = std::acos(-1.0);
    for (int y = 0; y < result.height(); ++y) {
        double const latitude = 90.0 - (180.0 * (y + 0.5) / result.height());
        double const clampedLatitude = qBound(-latitudeLimit, latitude, latitudeLimit);
        double const radians = qDegreesToRadians(clampedLatitude);
        double const mercatorY = (1.0 - std::asinh(std::tan(radians)) / pi) * 0.5;
        int const sourceY = qBound(0, qFloor(mercatorY * input.height()), input.height() - 1);
        QRgb* destination = reinterpret_cast<QRgb*>(result.scanLine(y));
        QRgb const* inputRow = reinterpret_cast<QRgb const*>(input.constScanLine(sourceY));
        for (int x = 0; x < result.width(); ++x) {
            int const sourceX = qBound(0, qFloor((x + 0.5) * input.width() / result.width()), input.width() - 1);
            destination[x] = inputRow[sourceX];
        }
    }
    return result;
}

void MapBaseMapService::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void MapBaseMapService::setStatus(const QString& status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void MapBaseMapService::applyImage(const QImage& image, const QString& status)
{
    if (!image.isNull()) {
        m_currentImage = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        emit baseMapImageChanged();
    }
    setLoading(false);
    setStatus(status);
}

void MapBaseMapService::loadCachedOnlineImage()
{
    QImage const cached(cacheFilePath());
    if (!cached.isNull()) {
        m_currentImage = cached.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        emit baseMapImageChanged();
        setStatus(QStringLiteral("Cached %1").arg(m_provider));
    }
}

void MapBaseMapService::requestNasaGibs()
{
    ++m_generation;
    int const generation = m_generation;
    setLoading(true);
    setStatus(QStringLiteral("Loading NASA GIBS satellite imagery"));

    QUrl url(QStringLiteral("https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("SERVICE"), QStringLiteral("WMS"));
    query.addQueryItem(QStringLiteral("VERSION"), QStringLiteral("1.1.1"));
    query.addQueryItem(QStringLiteral("REQUEST"), QStringLiteral("GetMap"));
    query.addQueryItem(QStringLiteral("LAYERS"), QStringLiteral("MODIS_Terra_CorrectedReflectance_TrueColor"));
    query.addQueryItem(QStringLiteral("STYLES"), QString());
    query.addQueryItem(QStringLiteral("SRS"), QStringLiteral("EPSG:4326"));
    query.addQueryItem(QStringLiteral("BBOX"), QStringLiteral("-180,-90,180,90"));
    query.addQueryItem(QStringLiteral("WIDTH"), QString::number(kOutputWidth));
    query.addQueryItem(QStringLiteral("HEIGHT"), QString::number(kOutputHeight));
    query.addQueryItem(QStringLiteral("FORMAT"), QStringLiteral("image/jpeg"));
    query.addQueryItem(QStringLiteral("TRANSPARENT"), QStringLiteral("FALSE"));
    query.addQueryItem(QStringLiteral("TIME"), QDate::currentDate().addDays(-1).toString(Qt::ISODate));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4 MapBase"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTimeoutMs);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        QByteArray const payload = reply->readAll();
        QString const error = reply->error() == QNetworkReply::NoError
            ? QString() : reply->errorString();
        reply->deleteLater();
        if (generation != m_generation || m_offlineMode
            || m_provider != QStringLiteral("NASA GIBS satellite")) {
            return;
        }
        QImage image;
        if (error.isEmpty() && payload.size() <= kMaxPayloadBytes) {
            image.loadFromData(payload);
        }
        if (image.isNull()) {
            setLoading(false);
            setStatus(error.isEmpty()
                ? QStringLiteral("NASA GIBS image unavailable; using cached/local atlas")
                : QStringLiteral("NASA GIBS: %1").arg(error));
            if (m_currentImage.isNull()) {
                applyImage(m_localAtlas, m_status);
            }
            return;
        }
        image = image.scaled(kOutputWidth, kOutputHeight,
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        saveOnlineCache(image);
        applyImage(image, QStringLiteral("NASA GIBS satellite updated %1 UTC")
                   .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("HH:mm"))));
    });
}

void MapBaseMapService::requestMapTilerTiles()
{
    if (m_mapTilerApiKey.isEmpty()) {
        setLoading(false);
        setStatus(QStringLiteral("MapTiler requires your API key"));
        return;
    }

    ++m_generation;
    int const generation = m_generation;
    int const count = 1 << m_tileZoom;
    m_tiles.clear();
    m_tileExpected = count * count;
    m_tileCompleted = 0;
    m_tileFailures = 0;
    setLoading(true);
    setStatus(QStringLiteral("Loading MapTiler satellite tiles"));

    for (int y = 0; y < count; ++y) {
        for (int x = 0; x < count; ++x) {
            QUrl url(QStringLiteral("https://api.maptiler.com/maps/satellite/%1/%2/%3.jpg")
                         .arg(m_tileZoom).arg(x).arg(y));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("key"), m_mapTilerApiKey);
            url.setQuery(query);
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4 MapBase"));
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setTransferTimeout(kNetworkTimeoutMs);
            QNetworkReply* reply = m_network->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, generation, x, y] {
                QByteArray const payload = reply->readAll();
                bool const success = reply->error() == QNetworkReply::NoError
                    && payload.size() <= kMaxPayloadBytes;
                reply->deleteLater();
                if (generation != m_generation || m_offlineMode
                    || m_provider != QStringLiteral("MapTiler satellite")) {
                    return;
                }
                QImage image;
                if (success) {
                    image.loadFromData(payload);
                }
                if (image.isNull()) {
                    ++m_tileFailures;
                } else {
                    m_tiles.insert(QStringLiteral("%1/%2").arg(x).arg(y), image);
                }
                ++m_tileCompleted;
                if (m_tileCompleted == m_tileExpected) {
                    finishMapTilerRequest();
                }
            });
        }
    }
}

void MapBaseMapService::finishMapTilerRequest()
{
    if (m_offlineMode || m_provider != QStringLiteral("MapTiler satellite")) {
        return;
    }
    int const count = 1 << m_tileZoom;
    if (m_tiles.isEmpty()) {
        setLoading(false);
        setStatus(QStringLiteral("MapTiler tiles unavailable; check API key"));
        return;
    }
    QSize tileSize;
    for (QImage const& image : std::as_const(m_tiles)) {
        if (!image.isNull()) {
            tileSize = image.size();
            break;
        }
    }
    if (tileSize.isEmpty()) {
        setLoading(false);
        setStatus(QStringLiteral("MapTiler returned no valid tiles"));
        return;
    }
    QImage mercator(tileSize.width() * count, tileSize.height() * count,
                    QImage::Format_ARGB32_Premultiplied);
    mercator.fill(QColor(4, 22, 38));
    QPainter painter(&mercator);
    for (int y = 0; y < count; ++y) {
        for (int x = 0; x < count; ++x) {
            QImage const tile = m_tiles.value(QStringLiteral("%1/%2").arg(x).arg(y));
            if (!tile.isNull()) {
                painter.drawImage(QRect(x * tileSize.width(), y * tileSize.height(),
                                        tileSize.width(), tileSize.height()), tile);
            }
        }
    }
    painter.end();
    QImage const image = webMercatorToEquirectangular(
        mercator, QSize(kOutputWidth, kOutputHeight));
    if (image.isNull()) {
        setLoading(false);
        setStatus(QStringLiteral("MapTiler projection failed"));
        return;
    }
    saveOnlineCache(image);
    applyImage(image, QStringLiteral("MapTiler satellite updated%1")
               .arg(m_tileFailures > 0
                    ? QStringLiteral(" (%1 tiles unavailable)").arg(m_tileFailures)
                    : QString()));
}

QString MapBaseMapService::cacheFilePath() const
{
    QString key = m_provider;
    if (m_provider == QStringLiteral("MapTiler satellite")) {
        key += QString::number(qHash(m_mapTilerApiKey));
    }
    key.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), QStringLiteral("_"));
    return QDir(m_cachePath).absoluteFilePath(key + QStringLiteral(".png"));
}

void MapBaseMapService::saveOnlineCache(const QImage& image) const
{
    if (!image.isNull()) {
        image.save(cacheFilePath(), "PNG");
    }
}
