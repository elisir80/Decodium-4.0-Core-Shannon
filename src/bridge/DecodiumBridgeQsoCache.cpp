#include "DecodiumBridge.h"
#include "DecodiumBridgeQsoCacheInternal.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>

extern int decodiumBridgeCountAdifRecords(const QString&);
extern void decodiumBridgeLogbookLog(const QString&);

int         DecodiumBridge::qsoCount()         const
{
    if (m_qsoCountCache < 0) {
        QElapsedTimer timer;
        timer.start();
        m_qsoCountCache = decodiumBridgeCountAdifRecords(effectiveAdifLogPath());
        if (timer.elapsed() > 100) {
            decodiumBridgeLogbookLog(QStringLiteral("qsoCount lightweight count: qsos=%1 elapsed=%2ms")
                          .arg(m_qsoCountCache)
                          .arg(timer.elapsed()));
        }
    }
    return m_qsoCountCache;
}

void DecodiumBridge::invalidateQsoSearchCache()
{
    m_qsoSearchCacheGeneration.fetch_add(1, std::memory_order_relaxed);
    QMutexLocker locker(&m_qsoSearchCacheMutex);
    m_qsoSearchCacheReady = false;
    m_qsoSearchCachePath.clear();
    m_qsoSearchCacheModified = QDateTime();
    m_qsoSearchCacheSize = -1;
    m_qsoSearchCacheRows.clear();
    m_qsoSearchCacheStats.clear();
}

void DecodiumBridge::warmLogCacheAsync()
{
    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchWarmupInProgress) {
            return;
        }
        m_qsoSearchWarmupInProgress = true;
    }

    QString const path = effectiveAdifLogPath();
    QString const grid = m_grid;
    quint64 const generation = m_qsoSearchCacheGeneration.load(std::memory_order_relaxed);
    QPointer<DecodiumBridge> guard(this);

    QThread* worker = QThread::create([guard, path, grid, generation]() {
        QsoLogSnapshot const snapshot = buildQsoLogSnapshot(path, grid);
        if (!guard) {
            return;
        }

        QMetaObject::invokeMethod(guard.data(), [guard, snapshot, generation]() {
            if (!guard) {
                return;
            }

            bool restartNeeded = false;
            {
                QMutexLocker locker(&guard->m_qsoSearchCacheMutex);
                if (generation == guard->m_qsoSearchCacheGeneration.load(std::memory_order_relaxed)) {
                    guard->m_qsoSearchCachePath = snapshot.path;
                    guard->m_qsoSearchCacheModified = snapshot.modified;
                    guard->m_qsoSearchCacheSize = snapshot.size;
                    guard->m_qsoSearchCacheRows = snapshot.rows;
                    guard->m_qsoSearchCacheStats = snapshot.stats;
                    guard->m_qsoSearchCacheReady = true;
                } else {
                    restartNeeded = true;
                }
                guard->m_qsoSearchWarmupInProgress = false;
            }

            if (restartNeeded) {
                QTimer::singleShot(0, guard.data(), [guard]() {
                    if (guard) {
                        guard->warmLogCacheAsync();
                    }
                });
                return;
            }
            emit guard->qsoLogCacheChanged();
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowestPriority);
}

QVariantList DecodiumBridge::searchQsos(const QString& search,
                                        const QString& band,
                                        const QString& mode,
                                        const QString& fromDate,
                                        const QString& toDate) const
{
    QElapsedTimer timer;
    timer.start();
    QString const path = effectiveAdifLogPath();
    QFileInfo const info(path);
    QDateTime const modified = info.exists() ? info.lastModified() : QDateTime();
    qint64 const size = info.exists() ? info.size() : 0;
    QVariantList sourceRows;
    bool cacheHit = false;

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        cacheHit = m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size;
        if (cacheHit) {
            sourceRows = m_qsoSearchCacheRows;
        }
    }

    if (!cacheHit) {
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            if (m_qsoSearchWarmupInProgress) {
                return {};
            }
        }

        QsoLogSnapshot const snapshot = buildQsoLogSnapshot(path, m_grid);
        {
            QMutexLocker locker(&m_qsoSearchCacheMutex);
            m_qsoSearchCachePath = snapshot.path;
            m_qsoSearchCacheModified = snapshot.modified;
            m_qsoSearchCacheSize = snapshot.size;
            m_qsoSearchCacheRows = snapshot.rows;
            m_qsoSearchCacheStats = snapshot.stats;
            m_qsoSearchCacheReady = true;
            sourceRows = m_qsoSearchCacheRows;
        }
    }

    QString const needle = search.trimmed().toUpper();
    QString const bandFilter = band.trimmed().toUpper();
    QString const modeFilter = mode.trimmed().toUpper();
    QString const fromFilter = fromDate.trimmed();
    QString const toFilter = toDate.trimmed();

    auto matchesDate = [] (QString const& dateTime, QString const& fromValue, QString const& toValue) {
        QString const compact = dateTime.left(10).remove(QLatin1Char('-'));
        QString const fromCompact = QString(fromValue).remove(QRegularExpression {R"([^0-9])"});
        QString const toCompact = QString(toValue).remove(QRegularExpression {R"([^0-9])"});
        if (!fromCompact.isEmpty() && compact < fromCompact) {
            return false;
        }
        if (!toCompact.isEmpty() && compact > toCompact) {
            return false;
        }
        return true;
    };

    if (needle.isEmpty() && bandFilter.isEmpty() && modeFilter.isEmpty()
        && fromFilter.isEmpty() && toFilter.isEmpty()) {
        return sourceRows;
    }

    QVariantList results;
    for (QVariant const& rowValue : sourceRows) {
        QVariantMap const row = rowValue.toMap();
        QString const rowBand = row.value(QStringLiteral("band")).toString().trimmed().toUpper();
        QString const rowMode = row.value(QStringLiteral("mode")).toString().trimmed().toUpper();
        QString const rowCall = row.value(QStringLiteral("call")).toString().trimmed().toUpper();
        QString const rowGrid = row.value(QStringLiteral("grid")).toString().trimmed().toUpper();
        QString const rowComment = row.value(QStringLiteral("comment")).toString().trimmed().toUpper();
        QString const rowDateTime = row.value(QStringLiteral("dateTime")).toString();

        if (!bandFilter.isEmpty() && rowBand != bandFilter) {
            continue;
        }
        if (!modeFilter.isEmpty() && rowMode != modeFilter) {
            continue;
        }
        if (!matchesDate(rowDateTime, fromFilter, toFilter)) {
            continue;
        }
        if (!needle.isEmpty()
            && !rowCall.contains(needle)
            && !rowGrid.contains(needle)
            && !rowComment.contains(needle)
            && !rowDateTime.toUpper().contains(needle)
            && !rowBand.contains(needle)
            && !rowMode.contains(needle)) {
            continue;
        }

        results.append(row);
    }
    if (timer.elapsed() > 250) {
        decodiumBridgeLogbookLog(QStringLiteral("searchQsos: cacheHit=%1 rows=%2 results=%3 elapsed=%4ms")
                      .arg(cacheHit)
                      .arg(sourceRows.size())
                      .arg(results.size())
                      .arg(timer.elapsed()));
    }
    return results;
}

QVariantMap DecodiumBridge::getQsoStats() const
{
    QString const path = effectiveAdifLogPath();
    QFileInfo const info(path);
    QDateTime const modified = info.exists() ? info.lastModified() : QDateTime();
    qint64 const size = info.exists() ? info.size() : 0;

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchCacheReady
            && m_qsoSearchCachePath == path
            && m_qsoSearchCacheModified == modified
            && m_qsoSearchCacheSize == size) {
            return m_qsoSearchCacheStats;
        }
    }

    {
        QMutexLocker locker(&m_qsoSearchCacheMutex);
        if (m_qsoSearchWarmupInProgress) {
            return {};
        }
    }

    QVariantList const rows = searchQsos(QString {}, QString {}, QString {}, QString {}, QString {});
    return qsoStatsFromRows(rows);
}

