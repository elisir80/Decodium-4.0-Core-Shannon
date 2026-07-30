#pragma once

#include <QImage>
#include <QHash>
#include <QObject>
#include <QSize>
#include <QStringList>

class QNetworkAccessManager;

// Owns the optional cartographic base layer. The radio/map overlays continue
// to use MapExternalOverlayService so switching imagery never affects them.
class MapBaseMapService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableProviders READ availableProviders CONSTANT)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(QString mapTilerApiKey READ mapTilerApiKey WRITE setMapTilerApiKey NOTIFY mapTilerApiKeyChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString attribution READ attribution NOTIFY providerChanged)
    Q_PROPERTY(QString attributionUrl READ attributionUrl NOTIFY providerChanged)

public:
    explicit MapBaseMapService(QObject* parent = nullptr,
                               const QString& cachePath = {});

    QStringList availableProviders() const;
    QString provider() const { return m_provider; }
    bool offlineMode() const { return m_offlineMode; }
    QString mapTilerApiKey() const { return m_mapTilerApiKey; }
    bool loading() const { return m_loading; }
    QString status() const { return m_status; }
    QString attribution() const;
    QString attributionUrl() const;
    QImage baseMapImage() const { return m_currentImage; }

    void setProvider(const QString& provider);
    void setOfflineMode(bool offline);
    void setMapTilerApiKey(const QString& apiKey);

    Q_INVOKABLE void refresh();

signals:
    void baseMapImageChanged();
    void providerChanged();
    void offlineModeChanged();
    void mapTilerApiKeyChanged();
    void loadingChanged();
    void statusChanged();

private:
    static QString normalizeProvider(const QString& provider);
    static QImage loadLocalAtlas();
    static QImage webMercatorToEquirectangular(const QImage& source,
                                               const QSize& outputSize);
    void setLoading(bool loading);
    void setStatus(const QString& status);
    void applyImage(const QImage& image, const QString& status);
    void loadCachedOnlineImage();
    void requestNasaGibs();
    void requestMapTilerTiles();
    void finishMapTilerRequest();
    QString cacheFilePath() const;
    void saveOnlineCache(const QImage& image) const;

    QNetworkAccessManager* m_network {nullptr};
    QImage m_localAtlas;
    QImage m_currentImage;
    QString m_provider {QStringLiteral("Decodium Atlas")};
    QString m_mapTilerApiKey;
    QString m_status;
    QString m_cachePath;
    bool m_offlineMode {false};
    bool m_loading {false};
    int m_generation {0};
    int m_tileExpected {0};
    int m_tileCompleted {0};
    int m_tileFailures {0};
    int m_tileZoom {2};
    QHash<QString, QImage> m_tiles;
};
