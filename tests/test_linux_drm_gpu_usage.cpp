#include <QtTest>

#include "src/bridge/LinuxDrmGpuUsage.h"

using decodium::gpu_usage::LinuxDrmFdInfo;

class TestLinuxDrmGpuUsage final : public QObject
{
    Q_OBJECT

private:
    static QString i915Snapshot(quint64 renderNs)
    {
        return QStringLiteral("drm-driver:\ti915\n"
                              "drm-client-id:\t46\n"
                              "drm-pdev:\t0000:00:02.0\n"
                              "drm-engine-render:\t%1 ns\n"
                              "drm-engine-copy:\t0 ns\n")
            .arg(renderNs);
    }

private slots:
    void duplicateDescriptorsAreCountedOnce()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("47"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("51"), i915Snapshot(8114572232ULL) },
            { QStringLiteral("52"), i915Snapshot(8114572232ULL) }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 8114572232ULL);
    }

    void distinctClientsAreAggregated()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(1000000000ULL) },
            { QStringLiteral("53"), QStringLiteral("drm-driver:\ti915\n"
                                                    "drm-client-id:\t47\n"
                                                    "drm-pdev:\t0000:00:02.0\n"
                                                    "drm-engine-copy:\t2000000000 ns\n") }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 3000000000ULL);
    }

    void highestDuplicateEngineCounterWins()
    {
        QList<LinuxDrmFdInfo> const descriptors {
            { QStringLiteral("46"), i915Snapshot(1000000000ULL) },
            { QStringLiteral("47"), i915Snapshot(1500000000ULL) }
        };

        quint64 totalNs = 0;
        QVERIFY(decodium::gpu_usage::aggregateLinuxDrmEngineTimeNs(descriptors, &totalNs));
        QCOMPARE(totalNs, 1500000000ULL);
    }
};

QTEST_MAIN(TestLinuxDrmGpuUsage)
#include "test_linux_drm_gpu_usage.moc"
