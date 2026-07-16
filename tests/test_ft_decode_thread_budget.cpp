#include "FtDecodeThreadBudget.hpp"

#include <QtTest>

class TestFtDecodeThreadBudget final : public QObject
{
    Q_OBJECT

private slots:
    void scalesAcrossCpuSizes_data();
    void scalesAcrossCpuSizes();
};

void TestFtDecodeThreadBudget::scalesAcrossCpuSizes_data()
{
    QTest::addColumn<int>("logicalCores");
    QTest::addColumn<int>("normalLimit");
    QTest::addColumn<int>("expected");

    QTest::newRow("single-core") << 1 << 1 << 1;
    QTest::newRow("dual-core") << 2 << 2 << 1;
    QTest::newRow("four-thread") << 4 << 3 << 3;
    QTest::newRow("six-thread") << 6 << 4 << 4;
    QTest::newRow("eight-thread-intel") << 8 << 6 << 5;
    QTest::newRow("ten-thread-apple") << 10 << 8 << 6;
    QTest::newRow("sixteen-thread") << 16 << 12 << 10;
    QTest::newRow("large-workstation") << 24 << 12 << 12;
    QTest::newRow("manual-low-limit") << 16 << 4 << 4;
    QTest::newRow("runtime-pressure-limit") << 10 << 5 << 5;
}

void TestFtDecodeThreadBudget::scalesAcrossCpuSizes()
{
    QFETCH(int, logicalCores);
    QFETCH(int, normalLimit);
    QFETCH(int, expected);

    QCOMPARE(decodium::decode::adaptiveInteractiveThreadCount(
                 logicalCores, normalLimit, 12),
             expected);
}

QTEST_MAIN(TestFtDecodeThreadBudget)
#include "test_ft_decode_thread_budget.moc"
