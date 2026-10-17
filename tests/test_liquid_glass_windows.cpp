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

} // namespace

class TestLiquidGlassWindows final : public QObject
{
    Q_OBJECT

private slots:
    void shellIsCrossPlatformAndThemeGated();
    void mainAndPopoutsUseGlassShell();
    void dialogsAndHostedPanelsUseGlassSurfaces();
    void restoredPopoutsWaitForStartupPresentation();
};

void TestLiquidGlassWindows::shellIsCrossPlatformAndThemeGated()
{
    const QString window = readSource(QStringLiteral("qml/decodium/components/GlassWindow.qml"));
    const QString applicationWindow = readSource(
        QStringLiteral("qml/decodium/components/GlassApplicationWindow.qml"));
    QVERIFY(!window.isEmpty());
    QVERIFY(!applicationWindow.isEmpty());

    for (const QString& shell : {window, applicationWindow}) {
        QVERIFY(shell.contains(QStringLiteral("isGlassTheme")));
        QVERIFY(shell.contains(QStringLiteral("glassTransparencySupported")));
        QVERIFY(shell.contains(QStringLiteral("Qt.rgba")));
        QVERIFY(!shell.contains(QStringLiteral("Qt.platform.os !== \"linux\"")));
        QVERIFY(shell.contains(QStringLiteral("default property alias windowContent")));
        // Do not make the Linux Qt 6.4 runtime depend on MultiEffect/native
        // material modules just to display the optional theme.
        QVERIFY(!shell.contains(QStringLiteral("import QtQuick.Effects")));
        QVERIFY(!shell.contains(QStringLiteral("MultiEffect {")));
    }
}

void TestLiquidGlassWindows::mainAndPopoutsUseGlassShell()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(!main.isEmpty());
    QVERIFY(main.startsWith(QStringLiteral("/* Decodium 4.0")));
    QVERIFY(main.contains(QStringLiteral("GlassApplicationWindow {")));
    QVERIFY(main.contains(QStringLiteral("id: waterfallWindow")));
    QVERIFY(main.contains(QStringLiteral("id: logFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("id: settingsFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("id: txPanelFloatingWindow")));
    QVERIFY(main.contains(QStringLiteral("property bool closeInProgress: false")));
    QVERIFY(main.contains(QStringLiteral("id: logNativePopupLoader")));
    QVERIFY(!main.contains(QStringLiteral("id: logWindowLoader")));
    QCOMPARE(main.count(QStringLiteral("source: \"components/LogWindow.qml\"")), 1);

    const QString liveMap = readSource(QStringLiteral("qml/decodium/components/LiveMapPanel.qml"));
    QVERIFY(!liveMap.isEmpty());
    QVERIFY(liveMap.contains(QStringLiteral("mapSurfaceOpacity")));
    QVERIFY(liveMap.contains(QStringLiteral("root.detached && !!(engine")));
    QVERIFY(liveMap.contains(QStringLiteral("opacity: root.mapSurfaceOpacity")));
    QVERIFY(liveMap.contains(QStringLiteral("detachedWindow")));
    QVERIFY(liveMap.contains(QStringLiteral("startSystemMove")));
    QVERIFY(liveMap.contains(QStringLiteral("root.detached ? \"transparent\" : root.bgDeep")));
    QVERIFY(main.contains(QStringLiteral("function glassWindowSurface(fallbackAlpha)")));
    QVERIFY(main.contains(QStringLiteral("function glassDialogSurface(fallbackAlpha)")));
    QVERIFY(main.contains(QStringLiteral("flags: Qt.Window | Qt.FramelessWindowHint")));

    const QString dxCluster = readSource(QStringLiteral("qml/decodium/components/DxClusterPanel.qml"));
    QVERIFY(!dxCluster.isEmpty());
    QVERIFY(dxCluster.contains(QStringLiteral("glassSurfaceEnabled")));
    QVERIFY(dxCluster.contains(QStringLiteral("!root.embedded && bridge.themeManager")));
    QVERIFY(dxCluster.contains(QStringLiteral("root.embedded\n                   ? root.bgDeep")));
    QVERIFY(dxCluster.contains(QStringLiteral("0.30")));
    const QString txPanel = readSource(QStringLiteral("qml/decodium/components/TxPanel.qml"));
    QVERIFY(txPanel.contains(QStringLiteral("property bool detachedSurface: false")));
    QVERIFY(txPanel.contains(QStringLiteral("txPanel.detachedSurface && engine")));
    const QString ft2Link = readSource(QStringLiteral("qml/panels/FT2LinkPanel.qml"));
    QVERIFY(ft2Link.contains(QStringLiteral("readonly property bool liquidGlassEnabled: poppedOut")));
    QVERIFY(main.contains(QStringLiteral("detachedSurface: true")));
    QVERIFY(main.contains(QStringLiteral("item.poppedOut = false")));
    QVERIFY(main.contains(QStringLiteral("item.poppedOut = true")));
    QVERIFY(main.contains(QStringLiteral("color: mainWindow.period1Detached ? \"transparent\" : bgDeep")));
    QVERIFY(main.contains(QStringLiteral("color: mainWindow.rxFreqDetached ? \"transparent\" : bgDeep")));
    QVERIFY(main.contains(QStringLiteral("The docked TX host is opaque")));
    QVERIFY(main.contains(QStringLiteral("id: dxcTitleBar")));
    QVERIFY(main.contains(QStringLiteral("Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint")));

    const QStringList popouts = {
        QStringLiteral("qml/decodium/components/CallDialog.qml"),
        QStringLiteral("qml/decodium/components/CwWindow.qml"),
        QStringLiteral("qml/decodium/components/DecodeHistoryDialog.qml"),
        QStringLiteral("qml/decodium/components/DecodeWindow.qml"),
        QStringLiteral("qml/decodium/components/DecoPortWindow.qml"),
        QStringLiteral("qml/decodium/components/DecolinkWindow.qml"),
        QStringLiteral("qml/decodium/components/DxPedFloatWindow.qml"),
        QStringLiteral("qml/decodium/components/JttyWindow.qml"),
        QStringLiteral("qml/decodium/components/QsyQuickPickerDialog.qml"),
        QStringLiteral("qml/decodium/components/SsbWindow.qml"),
        QStringLiteral("qml/decodium/components/RotorWindow.qml"),
        QStringLiteral("qml/decodium/components/sstv/SstvWorkspace.qml"),
        QStringLiteral("qml/decodium/rtty/RttyMain.qml")
    };
    for (const QString& path : popouts) {
        const QString source = readSource(path);
        QVERIFY2(!source.isEmpty(), qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("GlassWindow {"))
                         || source.contains(QStringLiteral("GlassApplicationWindow {")),
                  qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("glassSurfaceColor"))
                         || source.contains(QStringLiteral("glassTransparencySupported"))
                         || source.contains(QStringLiteral("liquidGlassTheme")),
                 qPrintable(path));
    }
}

void TestLiquidGlassWindows::dialogsAndHostedPanelsUseGlassSurfaces()
{
    const QStringList surfaces = {
        QStringLiteral("qml/decodium/CatSettingsDialog.qml"),
        QStringLiteral("qml/decodium/components/LogWindow.qml"),
        QStringLiteral("qml/decodium/components/LogWindowContent.qml"),
        QStringLiteral("qml/decodium/components/SettingsDialog.qml"),
        QStringLiteral("qml/decodium/components/StyledComboBox.qml"),
        QStringLiteral("qml/decodium/components/UpdateDialog.qml"),
        QStringLiteral("qml/decodium/components/BugReportDialog.qml"),
        QStringLiteral("qml/decodium/components/StationTelemetryDialog.qml"),
        QStringLiteral("qml/decodium/components/RigDetectResults.qml"),
        QStringLiteral("qml/dialogs/ColorHighlightingDialog.qml"),
        QStringLiteral("qml/dialogs/QSYDialog.qml"),
        QStringLiteral("qml/panels/ActiveStationsPanel.qml"),
        QStringLiteral("qml/panels/FT2LinkPanel.qml"),
        QStringLiteral("qml/panels/TimeSyncPanel.qml"),
        QStringLiteral("qml/decodium/components/DecoSyncPanel.qml"),
        QStringLiteral("qml/decodium/components/DecometerWindow.qml"),
        QStringLiteral("qml/decodium/components/AstroWindow.qml")
    };

    for (const QString& path : surfaces) {
        const QString source = readSource(path);
        QVERIFY2(!source.isEmpty(), qPrintable(path));
        QVERIFY2(source.contains(QStringLiteral("glassTransparencySupported"))
                     || source.contains(QStringLiteral("dialogSurface"))
                     || source.contains(QStringLiteral("popupSurface"))
                     || source.contains(QStringLiteral("liquidGlassEnabled")),
                 qPrintable(path));
    }
}

void TestLiquidGlassWindows::restoredPopoutsWaitForStartupPresentation()
{
    const QString main = readSource(QStringLiteral("qml/decodium/Main.qml"));
    QVERIFY(main.contains(QStringLiteral("property bool startupPresentationFinished: false")));
    QVERIFY(main.contains(QStringLiteral("function syncDxClusterFloatingVisibility(activate)")));
    QVERIFY(main.contains(QStringLiteral("suppressVisibleRestore")));
    QVERIFY(main.contains(QStringLiteral("\"dxClusterDetached\", \"dxClusterMinimized\", true)")));
    QVERIFY(main.contains(QStringLiteral("!startupPresentationFinished")));
    QVERIFY(main.contains(QStringLiteral("mainWindow.startupPresentationFinished\n                 && mainWindow.dxClusterPanelVisible")));
    QVERIFY(main.contains(QStringLiteral("mainWindow.finishStartupPresentation()")));
    QVERIFY(main.contains(QStringLiteral("visible: mainWindow.startupPresentationFinished\n                             && showWorldClock")));
}

QTEST_APPLESS_MAIN(TestLiquidGlassWindows)
#include "test_liquid_glass_windows.moc"
