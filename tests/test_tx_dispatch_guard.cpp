#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString functionBody(const QString& source, const QString& signature,
                     const QString& nextSignature)
{
    const int start = source.indexOf(signature);
    if (start < 0)
        return {};
    const int end = source.indexOf(nextSignature, start + signature.size());
    return source.mid(start, end > start ? end - start : -1);
}

} // namespace

class TestTxDispatchGuard final : public QObject
{
    Q_OBJECT

private slots:
    void commonGuardIsWiredIntoTxEntryPoints();
    void staleTx6RequiresAutoOrExplicitManualAuthority();
    void lifecycleTransitionsInvalidateDeferredRequests();
    void deferredCallbacksCheckRequestGeneration();
};

void TestTxDispatchGuard::commonGuardIsWiredIntoTxEntryPoints()
{
    const QString header = readSource(QStringLiteral("src/bridge/DecodiumBridge.h"));
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!header.isEmpty());
    QVERIFY(!cpp.isEmpty());

    QVERIFY(header.contains(QStringLiteral("void invalidateDeferredTxRequests(const QString& reason);")));
    QVERIFY(header.contains(QStringLiteral("bool txDispatchGuardAllows(const QString& reason);")));
    QVERIFY(header.contains(QStringLiteral("quint64 m_txRequestGeneration {0};")));
    QVERIFY(header.contains(QStringLiteral("int m_explicitManualTxRequest {0};")));

    const QString setTxEnabled = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setTxEnabled(bool v)"),
        QStringLiteral("bool DecodiumBridge::txEnabled() const"));
    const QString startTx = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::startTx()"),
        QStringLiteral("void DecodiumBridge::sendTx(int n)"));
    const QString periodicTx = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::checkAndStartPeriodicTx()"),
        QStringLiteral("bool DecodiumBridge::isAutoCqBurstPureCq"));

    QVERIFY(setTxEnabled.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(startTx.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(periodicTx.contains(QStringLiteral("txDispatchGuardAllows")));
    QVERIFY(cpp.contains(QStringLiteral("armExplicitManualTxRequest(n, QStringLiteral(\"TX panel\"))")));
    QVERIFY(cpp.contains(QStringLiteral("armExplicitManualTxRequest(newCurrentTx")));
}

void TestTxDispatchGuard::staleTx6RequiresAutoOrExplicitManualAuthority()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString guard = functionBody(
        cpp,
        QStringLiteral("bool DecodiumBridge::txDispatchGuardAllows"),
        QStringLiteral("void DecodiumBridge::invalidateDeferredTxRequests"));
    QVERIFY(!guard.isEmpty());
    QVERIFY(guard.contains(QStringLiteral("m_currentTx == 6")));
    QVERIFY(guard.contains(QStringLiteral("!m_autoCqRepeat")));
    QVERIFY(guard.contains(QStringLiteral("!m_multiAnswerMode")));
    QVERIFY(guard.contains(QStringLiteral("explicitManual")));
    QVERIFY(guard.contains(QStringLiteral("m_txEnabled = false")));
    QVERIFY(guard.contains(QStringLiteral("blocked stale TX6")));
}

void TestTxDispatchGuard::lifecycleTransitionsInvalidateDeferredRequests()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString autoCq = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setAutoCqRepeat(bool v)"),
        QStringLiteral("bool DecodiumBridge::autoCallModeSupported() const"));
    const QString halt = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::haltWithReason(const QString& reason)"),
        QStringLiteral("void DecodiumBridge::setTune(bool v)"));
    const QString resetQso = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::resetQsoStateForNextSequence(bool preserveAutoTx)"),
        QStringLiteral("bool DecodiumBridge::finishAutoSequenceQsoFromCompletedSignoff"));
    const QString completedQso = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::clearTxArmedAfterCompletedQso"),
        QStringLiteral("void DecodiumBridge::logQsoNow()"));

    QVERIFY(autoCq.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(halt.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(resetQso.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(completedQso.contains(QStringLiteral("invalidateDeferredTxRequests")));
    QVERIFY(cpp.contains(QStringLiteral("++m_txRequestGeneration")));
    QVERIFY(cpp.contains(QStringLiteral("m_periodicTxCheckScheduled = false")));
    QVERIFY(cpp.contains(QStringLiteral("m_syncTxRetryScheduled = false")));
}

void TestTxDispatchGuard::deferredCallbacksCheckRequestGeneration()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    // Every timer-backed TX scheduler must capture the lifecycle generation
    // and drop its callback after HALT/QSO completion invalidates it.
    const int generationChecks = cpp.count(
        QStringLiteral("requestGeneration != m_txRequestGeneration"));
    QVERIFY2(generationChecks >= 8,
             qPrintable(QStringLiteral("only %1 deferred TX generation checks found")
                            .arg(generationChecks)));
    QVERIFY(cpp.contains(QStringLiteral("tryStartDeferredManualSyncTx")));
    QVERIFY(cpp.contains(QStringLiteral("TX guard")));
}

QTEST_MAIN(TestTxDispatchGuard)
#include "test_tx_dispatch_guard.moc"
