#include <QtTest>
#include "Sequencer/AutoCqCallPolicy.hpp"

class TestAutoCqCallPolicy : public QObject {
    Q_OBJECT
private slots:
    void customAndStandardCalls() {
        for (const auto& text : {"CQ VY2XT FN86", "QRZ VY2XT", "TEST VY2XT FN86"})
            QVERIFY(decodium::isAutoCqCall(true, 6, true, false, text));
    }
    void repliesAreNotCalls() {
        for (int tx = 1; tx < 6; ++tx)
            QVERIFY(!decodium::isAutoCqCall(true, tx, true, false, "TEST VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, true, true, "CQ VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, false, false, "CQ VY2XT FN86"));
    }
    void inactiveAndEmptyAreNotCalls() {
        QVERIFY(!decodium::isAutoCqCall(false, 6, true, false, "TEST VY2XT FN86"));
        QVERIFY(!decodium::isAutoCqCall(true, 6, true, false, "  "));
    }
};
QTEST_GUILESS_MAIN(TestAutoCqCallPolicy)
#include "test_auto_cq_call_policy.moc"
