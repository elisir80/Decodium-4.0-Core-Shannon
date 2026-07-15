#include "DecodeListModel.h"

#include <QSignalSpy>
#include <QtTest>

namespace {
QVariantMap decodeRow(QString const& time, QString const& message, QString const& db = QStringLiteral("-10"))
{
    return {
        {QStringLiteral("time"), time},
        {QStringLiteral("db"), db},
        {QStringLiteral("dt"), QStringLiteral("0.1")},
        {QStringLiteral("freq"), QStringLiteral("1500")},
        {QStringLiteral("message"), message},
        {QStringLiteral("isTx"), false}
    };
}

QVariantList rows(std::initializer_list<QVariantMap> values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (QVariantMap const& value : values) result.append(value);
    return result;
}
}

class TestDecodeListModel final : public QObject
{
    Q_OBJECT

private slots:
    void appendAndShiftStayIncremental();
    void prependAndTailPruneStayIncremental();
    void provisionalTailReplacementKeepsHistoryIncremental();
    void highVolumePassReplacementAvoidsReset();
};

void TestDecodeListModel::appendAndShiftStayIncremental()
{
    DecodeListModel model;
    model.setEntries(rows({decodeRow("120000", "CQ A1AAA AA00"),
                           decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120030", "CQ C1CCC CC22")}));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.setEntries(rows({decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120030", "CQ C1CCC CC22"),
                           decodeRow("120045", "CQ D1DDD DD33")}));

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(model.entry(0).value("message").toString(), QStringLiteral("CQ B1BBB BB11"));
    QCOMPARE(model.entry(2).value("message").toString(), QStringLiteral("CQ D1DDD DD33"));
}

void TestDecodeListModel::prependAndTailPruneStayIncremental()
{
    DecodeListModel model;
    model.setEntries(rows({decodeRow("120030", "CQ C1CCC CC22"),
                           decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120000", "CQ A1AAA AA00"),
                           decodeRow("115945", "CQ Z1ZZZ ZZ99")}));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.setEntries(rows({decodeRow("120100", "CQ E1EEE EE44"),
                           decodeRow("120045", "CQ D1DDD DD33"),
                           decodeRow("120030", "CQ C1CCC CC22"),
                           decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120000", "CQ A1AAA AA00")}));

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.entry(0).value("message").toString(), QStringLiteral("CQ E1EEE EE44"));
    QCOMPARE(model.entry(4).value("message").toString(), QStringLiteral("CQ A1AAA AA00"));
}

void TestDecodeListModel::provisionalTailReplacementKeepsHistoryIncremental()
{
    DecodeListModel model;
    model.setEntries(rows({decodeRow("120000", "CQ A1AAA AA00"),
                           decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120030", "CQ EARLY1 EE11"),
                           decodeRow("120030", "CQ EARLY2 EE22")}));

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.setEntries(rows({decodeRow("120000", "CQ A1AAA AA00"),
                           decodeRow("120015", "CQ B1BBB BB11"),
                           decodeRow("120030", "CQ FINAL1 FF11"),
                           decodeRow("120030", "CQ FINAL2 FF22"),
                           decodeRow("120030", "CQ FINAL3 FF33")}));

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(model.rowCount(), 5);
    QCOMPARE(model.entry(0).value("message").toString(), QStringLiteral("CQ A1AAA AA00"));
    QCOMPARE(model.entry(2).value("message").toString(), QStringLiteral("CQ FINAL1 FF11"));
    QCOMPARE(model.entry(4).value("message").toString(), QStringLiteral("CQ FINAL3 FF33"));
}

void TestDecodeListModel::highVolumePassReplacementAvoidsReset()
{
    QVariantList earlyRows;
    earlyRows.reserve(500);
    for (int i = 0; i < 500; ++i) {
        earlyRows.append(decodeRow(QString::number(120000 + i),
                                   QStringLiteral("EARLY ROW %1").arg(i)));
    }

    DecodeListModel model;
    model.setEntries(earlyRows);

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy insertSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removeSpy(&model, &QAbstractItemModel::rowsRemoved);

    QVariantList finalRows = earlyRows.mid(0, 450);
    finalRows.reserve(510);
    for (int i = 0; i < 60; ++i) {
        finalRows.append(decodeRow(QString::number(120450 + i),
                                   QStringLiteral("FINAL ROW %1").arg(i)));
    }
    model.setEntries(finalRows);

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(removeSpy.count(), 1);
    QCOMPARE(insertSpy.count(), 1);
    QCOMPARE(model.rowCount(), 510);
    QCOMPARE(model.entry(449).value("message").toString(), QStringLiteral("EARLY ROW 449"));
    QCOMPARE(model.entry(450).value("message").toString(), QStringLiteral("FINAL ROW 0"));
}

QTEST_MAIN(TestDecodeListModel)
#include "test_decode_list_model.moc"
