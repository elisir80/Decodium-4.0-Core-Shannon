#pragma once

#include <QHash>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QtGlobal>

#include <limits>

namespace decodium::gpu_usage {

// A process can expose the same DRM client through several file descriptors.
// Each fdinfo copy contains cumulative engine time for that client, so callers
// must keep the greatest value per engine rather than summing duplicate FDs.
struct LinuxDrmFdInfo
{
    QString descriptor;
    QString contents;
};

inline bool aggregateLinuxDrmEngineTimeNs(QList<LinuxDrmFdInfo> const& descriptors,
                                          quint64* gpuTimeNs)
{
    static QRegularExpression const driverRe(QStringLiteral("^drm-driver:\\s*(.+)$"));
    static QRegularExpression const clientIdRe(QStringLiteral("^drm-client-id:\\s*(.+)$"));
    static QRegularExpression const pdevRe(QStringLiteral("^drm-pdev:\\s*(.+)$"));
    static QRegularExpression const engineTimeRe(
        QStringLiteral("^drm-engine-([^:]+):\\s*(\\d+)\\s*([a-zA-Z]*)"));

    QHash<QString, QHash<QString, quint64>> maxEngineTimeByClient;
    bool found = false;

    for (LinuxDrmFdInfo const& descriptor : descriptors) {
        QString driver;
        QString clientId;
        QString pdev;
        QHash<QString, quint64> engineTimes;

        for (QString const& rawLine : descriptor.contents.split(QLatin1Char('\n'))) {
            QString const line = rawLine.trimmed();
            QRegularExpressionMatch const driverMatch = driverRe.match(line);
            if (driverMatch.hasMatch()) {
                driver = driverMatch.captured(1).trimmed();
                continue;
            }
            QRegularExpressionMatch const clientIdMatch = clientIdRe.match(line);
            if (clientIdMatch.hasMatch()) {
                clientId = clientIdMatch.captured(1).trimmed();
                continue;
            }
            QRegularExpressionMatch const pdevMatch = pdevRe.match(line);
            if (pdevMatch.hasMatch()) {
                pdev = pdevMatch.captured(1).trimmed();
                continue;
            }

            QRegularExpressionMatch const engineMatch = engineTimeRe.match(line);
            if (!engineMatch.hasMatch())
                continue;

            bool ok = false;
            quint64 value = engineMatch.captured(2).toULongLong(&ok);
            if (!ok)
                continue;

            QString const unit = engineMatch.captured(3).toLower();
            if (unit == QStringLiteral("us"))
                value *= 1000ULL;
            else if (unit == QStringLiteral("ms"))
                value *= 1000000ULL;

            QString const engine = engineMatch.captured(1);
            engineTimes[engine] = qMax(engineTimes.value(engine), value);
        }

        if (engineTimes.isEmpty())
            continue;

        // Old kernels might not expose a client id.  In that case retain the
        // descriptor as an independent source rather than merging unrelated
        // clients that happen to use the same driver.
        QString const clientKey = clientId.isEmpty()
            ? QStringLiteral("fd:%1").arg(descriptor.descriptor)
            : driver + QChar(0x1f) + clientId + QChar(0x1f) + pdev;
        QHash<QString, quint64>& maxEngineTime = maxEngineTimeByClient[clientKey];
        for (auto it = engineTimes.cbegin(); it != engineTimes.cend(); ++it) {
            maxEngineTime[it.key()] = qMax(maxEngineTime.value(it.key()), it.value());
        }
        found = true;
    }

    if (!found)
        return false;

    quint64 totalNs = 0;
    for (auto client = maxEngineTimeByClient.cbegin(); client != maxEngineTimeByClient.cend(); ++client) {
        for (auto engine = client->cbegin(); engine != client->cend(); ++engine) {
            if (std::numeric_limits<quint64>::max() - totalNs < engine.value()) {
                totalNs = std::numeric_limits<quint64>::max();
                break;
            }
            totalNs += engine.value();
        }
    }

    if (gpuTimeNs)
        *gpuTimeNs = totalNs;
    return true;
}

} // namespace decodium::gpu_usage
