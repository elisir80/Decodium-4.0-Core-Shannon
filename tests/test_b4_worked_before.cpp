#include <QFile>
#include <QString>
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

class TestB4WorkedBefore final : public QObject
{
    Q_OBJECT

private slots:
    void modernDecodePanelsRenderStrikethrough();
    void settingPersistsOnlyOnUserToggle();
    void successfulLogPathsRefreshWorkedBeforeRows();
};

void TestB4WorkedBefore::modernDecodePanelsRenderStrikethrough()
{
    const QStringList panels {
        QStringLiteral("qml/decodium/components/FullSpectrumPanel.qml"),
        QStringLiteral("qml/decodium/components/SignalRxPanel.qml")
    };
    for (const QString& panel : panels) {
        const QString source = readSource(panel);
        QVERIFY2(!source.isEmpty(), qPrintable(panel));
        QVERIFY2(source.contains(QStringLiteral("font.strikeout:")), qPrintable(panel));
        QVERIFY2(source.contains(QStringLiteral("del.entry.isB4 === true")), qPrintable(panel));
        QVERIFY2(source.contains(QStringLiteral("del.entry.dxIsWorked === true")), qPrintable(panel));
        QVERIFY2(source.contains(QStringLiteral("root.bridge.b4Strikethrough")), qPrintable(panel));
    }
}

void TestB4WorkedBefore::settingPersistsOnlyOnUserToggle()
{
    const QString qml = readSource(
        QStringLiteral("qml/decodium/components/SettingsTab8.qml"));
    QVERIFY(!qml.isEmpty());
    const int label = qml.indexOf(QStringLiteral("B4 Strikethrough:"));
    QVERIFY(label >= 0);
    const QString block = qml.mid(label, 900);
    QVERIFY(block.contains(
        QStringLiteral("onToggled: bridge.b4Strikethrough = checked")));
    QVERIFY(!block.contains(QStringLiteral("onCheckedChanged:")));
    QVERIFY(!block.contains(QStringLiteral("bridge.setSetting(\"b4Strikethrough\"")));

    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    const QString setter = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::setB4Strikethrough(bool enabled)"),
        QStringLiteral("void DecodiumBridge::setDecodeColorEnabled"));
    QVERIFY(!setter.isEmpty());
    QVERIFY(setter.contains(QStringLiteral("beginActiveSettingsProfile(settings)")));
    QVERIFY(setter.contains(
        QStringLiteral("settings.setValue(QStringLiteral(\"b4Strikethrough\"), enabled)")));
    QVERIFY(setter.contains(QStringLiteral("settings.sync()")));
}

void TestB4WorkedBefore::successfulLogPathsRefreshWorkedBeforeRows()
{
    const QString cpp = readSource(QStringLiteral("src/bridge/DecodiumBridge.cpp"));
    QVERIFY(!cpp.isEmpty());

    const QString legacyPath = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::logQsoNow()"),
        QStringLiteral("QString DecodiumBridge::logAllTxtPath() const"));
    QVERIFY(!legacyPath.isEmpty());
    const int legacyAppend = legacyPath.indexOf(
        QStringLiteral("appendWorkedQso(legacyCompletedCall"));
    const int legacyRefresh = legacyPath.indexOf(
        QStringLiteral("refreshWorkedBeforeDecodeEntriesForCall(legacyCompletedCall)"));
    const int snapshotClear = legacyPath.indexOf(
        QStringLiteral("clearPromptLogSnapshot()"), legacyAppend);
    QVERIFY(legacyAppend >= 0);
    QVERIFY(legacyRefresh > legacyAppend);
    QVERIFY(snapshotClear > legacyRefresh);

    const QString nativePath = functionBody(
        cpp,
        QStringLiteral("void DecodiumBridge::appendAdifRecord("),
        QStringLiteral("QVariantList DecodiumBridge::logbookProfiles() const"));
    QVERIFY(!nativePath.isEmpty());
    const int nativeAppend = nativePath.indexOf(
        QStringLiteral("appendWorkedQso(dxCall"));
    const int nativeRefresh = nativePath.indexOf(
        QStringLiteral("refreshWorkedBeforeDecodeEntriesForCall(dxCall)"));
    QVERIFY(nativeAppend >= 0);
    QVERIFY(nativeRefresh > nativeAppend);
}

QTEST_MAIN(TestB4WorkedBefore)
#include "test_b4_worked_before.moc"
