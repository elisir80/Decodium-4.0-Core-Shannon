/* DxPeditionWorkspace — DX-Pedition Mode 3-column tactical layout
 * Phase 2a scaffold (1.0.330): layout shell + easy reused panels + placeholders.
 * Opt-in alternative workspace; loaded only when mainWindow.dxPeditionMode is ON.
 * Design source of truth: dxped_handoff/README.md (§2 layout, §3 components),
 *   dx-pedition-mode.html. Colours/metrics come ONLY from theme tokens (Fase 1).
 * Phase 2b/3 will replace the Full Spectrum / Signal RX / PSK / Log placeholders.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: workspace

    // Passed from Main.qml. Children read global `bridge` directly too, but we keep
    // explicit handles so this component is self-describing / testable in isolation.
    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)
    property var engine: (typeof appEngine !== 'undefined' ? appEngine : null)

    // Wired by Main.qml's Loader.onLoaded — let the user leave the mode or open
    // Settings from inside the workspace (the classic footer/menu is collapsed here,
    // so without these the user would be trapped). See Main.qml dxPeditionLoader.
    signal requestExitDxPedition()
    signal requestOpenSettings()
    signal requestOpenLog()    // 1.0.344 — pulsante LOG header → finestra QSO Log
    signal requestOpenMam()    // 1.0.344 — finestra MAM (Multi-Answer Mode)
    signal requestOpenMacro()  // 1.0.345 — Macro Dialog (editor messaggi)
    signal requestOpenCat()    // 1.0.345 — CAT / Rig Control dialog

    // --- Theme token shortcuts (Fase 1) — NO hardcoded hex anywhere below ---------
    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:     tm ? tm.accentColor   : "#19ff88"
    readonly property color cAccentDim:  tm ? tm.accentDim     : "#0fa55a"
    readonly property color cAccentDeep: tm ? tm.accentDeep    : "#052d1a"
    readonly property color cPanel:      tm ? tm.panelColor    : "#0d1310"
    readonly property color cPanelHdr:   tm ? tm.panelHeader   : "#0a0e0c"
    readonly property color cBorder:     tm ? tm.borderColor   : "#1f2a22"
    readonly property color cBg:         tm ? tm.bgDeep        : "#050706"
    readonly property color cText:       tm ? tm.textPrimary   : "#d6dcd8"
    readonly property color cTextDim:    tm ? tm.textSecondary : "#6c7872"
    readonly property color cPile:       tm ? tm.pileColor     : "#66e6ff"
    readonly property color cGrid:       tm ? tm.gridColor     : "#00d4b4"
    readonly property color cTx:         tm ? tm.txColor       : "#ff7a5c"
    readonly property color cRx:         tm ? tm.rxColor       : "#19ff88"
    readonly property color cWarn:       tm ? tm.warningColor  : "#ffb84a"
    readonly property color cHot:        tm ? tm.errorColor    : "#ff5466"

    // Density metrics (Fase 1 invokables) with safe fallbacks.
    readonly property int densPanelH: tm ? tm.densityPanelHeight() : 30
    readonly property int densRowH:   tm ? tm.densityRowHeight()   : 22
    readonly property int densFont:   tm ? tm.densityFontSize()    : 12

    // Background fill so the classic chrome never shows through behind the shell.
    Rectangle { anchors.fill: parent; color: workspace.cBg }

    // ---------------------------------------------------------------------------
    // Reusable styled panel: rounded Rectangle + 30px header + body slot.
    // ---------------------------------------------------------------------------
    component DxPanel: Rectangle {
        id: panel
        property string title: ""
        property string meta: ""
        property bool live: false
        property color accent: workspace.cAccent
        default property alias content: bodyHolder.data

        radius: 10
        color: workspace.cPanel
        border.color: workspace.cBorder
        border.width: 1
        clip: true

        // Header (panelHeader 30px, uppercase 10px accent title).
        Rectangle {
            id: hdr
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: workspace.densPanelH
            color: workspace.cPanelHdr
            radius: 10
            // Square the bottom corners.
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 10; color: parent.color
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 8
                Rectangle {
                    visible: panel.live
                    width: 8; height: 8; radius: 4
                    color: panel.accent
                    SequentialAnimation on opacity {
                        running: panel.live; loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.25; duration: 800 }
                        NumberAnimation { from: 0.25; to: 1.0; duration: 800 }
                    }
                }
                Text {
                    text: panel.title.toUpperCase()
                    color: panel.accent
                    font.pixelSize: 10
                    font.bold: true
                    font.letterSpacing: 1.4
                    Layout.alignment: Qt.AlignVCenter
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: panel.meta
                    visible: panel.meta.length > 0
                    color: workspace.cTextDim
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        // Body slot.
        Item {
            id: bodyHolder
            anchors { left: parent.left; right: parent.right; top: hdr.bottom; bottom: parent.bottom }
            anchors.margins: 1
        }
    }

    // A neutral "panel placeholder" body — used until the real component is extracted.
    component PanelStub: Item {
        property string note: "panel"
        Text {
            anchors.centerIn: parent
            text: parent.note
            color: workspace.cTextDim
            font.pixelSize: 11
            font.italic: true
        }
    }

    // ===========================================================================
    // Outer 4-row stack: header 60 / band 32 / main 1fr / status 28, gap 12, pad 12
    // ===========================================================================
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // -------- ROW 1 — HEADER (60px) ----------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 10
            color: workspace.cPanel
            border.color: workspace.cBorder
            border.width: 1

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 16

                // Brand block.
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 26; height: 26; radius: 6
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: workspace.cAccent }
                            GradientStop { position: 1.0; color: workspace.cAccentDim }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "DX"; color: workspace.cBg
                            font.pixelSize: 11; font.bold: true
                        }
                    }
                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: "DECODIUM 4"
                            color: workspace.cText
                            font.pixelSize: 14; font.bold: true; font.letterSpacing: 1.4
                        }
                        Text {
                            text: "DX-PEDITION MODE"
                            color: workspace.cTextDim
                            font.pixelSize: 9; font.letterSpacing: 1.6
                        }
                    }
                }

                // Mode pill.
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: modeLbl.implicitWidth + 16
                    implicitHeight: 20
                    radius: 4
                    color: workspace.cAccentDeep
                    border.color: workspace.cAccentDim
                    border.width: 1
                    Text {
                        id: modeLbl
                        anchors.centerIn: parent
                        text: (workspace.bridge && workspace.bridge.mode ? String(workspace.bridge.mode) : "FT8")
                        color: workspace.cAccent
                        font.pixelSize: 10; font.bold: true
                        font.family: "monospace"
                    }
                }

                // Large frequency display.
                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: {
                        var hz = workspace.bridge && workspace.bridge.dialFrequency !== undefined
                                 ? Number(workspace.bridge.dialFrequency) : 0
                        if (!hz && workspace.bridge && workspace.bridge.frequency !== undefined)
                            hz = Number(workspace.bridge.frequency)
                        if (!hz) return "—.———.———"
                        var s = String(Math.round(hz))
                        // group thousands with dots: 18108000 -> 18.108.000
                        var out = ""; var c = 0
                        for (var i = s.length - 1; i >= 0; i--) {
                            out = s.charAt(i) + out
                            if (++c % 3 === 0 && i > 0) out = "." + out
                        }
                        return out
                    }
                    color: workspace.cAccent
                    font.pixelSize: 36; font.bold: true
                    font.family: "monospace"
                }

                Item { Layout.fillWidth: true }

                // UTC clock.
                ColumnLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignVCenter
                    Text {
                        id: utcClock
                        property string t: "00:00:00"
                        text: t
                        color: workspace.cAccent
                        font.pixelSize: 22; font.bold: true; font.family: "monospace"
                        Timer {
                            interval: 1000; running: workspace.visible; repeat: true; triggeredOnStart: true
                            onTriggered: {
                                var d = new Date()
                                function p(n) { return (n < 10 ? "0" : "") + n }
                                utcClock.t = p(d.getUTCHours()) + ":" + p(d.getUTCMinutes()) + ":" + p(d.getUTCSeconds())
                            }
                        }
                    }
                    Text {
                        text: "UTC · OPER IU8LMC"
                        color: workspace.cTextDim
                        font.pixelSize: 10; font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                    }
                }

                // Mini toolbar. SETUP opens Settings; others are placeholders (2b).
                RowLayout {
                    spacing: 6
                    Layout.alignment: Qt.AlignVCenter
                    Repeater {
                        model: ["SETUP", "LOG", "MAM", "MACRO", "CAT"]
                        delegate: Rectangle {
                            required property string modelData
                            implicitWidth: tbTxt.implicitWidth + 16
                            implicitHeight: 32
                            radius: 6
                            color: tbMA.containsMouse ? workspace.cAccentDeep : "transparent"
                            border.color: workspace.cBorder
                            border.width: 1
                            Text {
                                id: tbTxt
                                anchors.centerIn: parent
                                text: parent.modelData
                                color: tbMA.containsMouse ? workspace.cAccent : workspace.cTextDim
                                font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.0
                            }
                            MouseArea {
                                id: tbMA; anchors.fill: parent; hoverEnabled: true
                                // 1.0.344 — SETUP/LOG/MAM cablati; MACRO/CAT placeholder.
                                // 1.0.345 — tutti i pulsanti header cablati.
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (parent.modelData === "SETUP")
                                        workspace.requestOpenSettings()
                                    else if (parent.modelData === "LOG")
                                        workspace.requestOpenLog()
                                    else if (parent.modelData === "MAM")
                                        workspace.requestOpenMam()
                                    else if (parent.modelData === "MACRO")
                                        workspace.requestOpenMacro()
                                    else if (parent.modelData === "CAT")
                                        workspace.requestOpenCat()
                                }
                            }
                        }
                    }

                    // EXIT — leave DX-Pedition Mode and return to the classic layout.
                    // Without this the user is trapped (classic footer/menu is hidden).
                    Rectangle {
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: exitTxt.implicitWidth + 22
                        implicitHeight: 32
                        radius: 6
                        color: exitMA.containsMouse ? workspace.cHot : "transparent"
                        border.color: workspace.cHot
                        border.width: 1
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 5
                            Text {
                                text: "✕"
                                color: exitMA.containsMouse ? workspace.cBg : workspace.cHot
                                font.pixelSize: 12; font.bold: true
                            }
                            Text {
                                id: exitTxt
                                text: "EXIT"
                                color: exitMA.containsMouse ? workspace.cBg : workspace.cHot
                                font.pixelSize: 11; font.bold: true; font.letterSpacing: 1.0
                            }
                        }
                        MouseArea {
                            id: exitMA; anchors.fill: parent; hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: workspace.requestExitDxPedition()
                        }
                    }
                }
            }
        }

        // -------- ROW 2 — BAND STRIP (32px) ------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            radius: 8
            color: workspace.cPanel
            border.color: workspace.cBorder
            border.width: 1

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                spacing: 6

                Text {
                    text: "BAND"
                    color: workspace.cTextDim
                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.6
                    Layout.alignment: Qt.AlignVCenter
                }

                Repeater {
                    model: ["160","80","60","40","30","20","17","15","12","10","6","4","2","70"]
                    delegate: Rectangle {
                        required property string modelData
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: 34
                        implicitHeight: 24
                        radius: 4
                        property bool active: false   // wired to current band in 2b
                        color: active ? workspace.cAccentDeep
                                      : (bandMA.containsMouse ? workspace.cPanelHdr : "transparent")
                        border.color: active ? workspace.cAccentDim : workspace.cBorder
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: parent.active ? workspace.cAccent : workspace.cTextDim
                            font.pixelSize: 11; font.bold: true; font.family: "monospace"
                        }
                        MouseArea {
                            id: bandMA; anchors.fill: parent; hoverEnabled: true
                            onClicked: {
                                // Placeholder: real band switch (CAT) wired in 2b.
                                if (workspace.bridge && typeof workspace.bridge.setBand === "function")
                                    workspace.bridge.setBand(parent.modelData)
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Two status chips (placeholder text — real cycle/seq state in 2b).
                Repeater {
                    model: ["● RX ACTIVE", "● AUTO-SEQ ON"]
                    delegate: Rectangle {
                        required property string modelData
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: chipTxt.implicitWidth + 14
                        implicitHeight: 22
                        radius: 4
                        color: workspace.cPanelHdr
                        border.color: workspace.cBorder
                        border.width: 1
                        Text {
                            id: chipTxt
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: workspace.cTextDim
                            font.pixelSize: 10; font.family: "monospace"
                        }
                    }
                }
            }
        }

        // -------- ROW 3 — MAIN 3 COLUMNS (resizable) ---------------------------
        // 1.0.342 — SplitView annidati: orizzontale per le 3 colonne (resize larghezza),
        // verticale dentro ogni colonna (resize altezza pannelli). Maniglie trascinabili.
        SplitView {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                color: SplitHandle.pressed ? workspace.cAccent
                     : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                Rectangle {
                    anchors.centerIn: parent
                    width: 2; height: 28; radius: 1
                    color: workspace.cBorder
                }
            }

            // ---- COL LEFT : Cluster 1.4 / PSK 1 -------------------------------
            SplitView {
                id: colLeftSplit
                orientation: Qt.Vertical
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 240
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxPanel {
                    id: clusterMamPanel
                    SplitView.preferredHeight: 360
                    SplitView.minimumHeight: 120
                    title: clusterMamPanel.leftTab === 0 ? "Cluster" : "MAM"
                    meta: "live feed"
                    live: true
                    // 1.0.345 — tab Cluster|MAM: 0=DX Cluster, 1=Multi-Answer Mode.
                    property int leftTab: 0

                    // Selettore tab in cima al body del pannello.
                    Row {
                        id: clusterMamTabs
                        anchors { top: parent.top; left: parent.left; right: parent.right; topMargin: 2 }
                        height: 26
                        spacing: 6
                        Repeater {
                            model: ["CLUSTER", "MAM"]
                            delegate: Rectangle {
                                required property int index
                                required property string modelData
                                width: (clusterMamTabs.width - 6) / 2
                                height: 24
                                radius: 4
                                readonly property bool sel: clusterMamPanel.leftTab === index
                                color: sel ? workspace.cAccentDeep
                                           : (tabMA.containsMouse ? workspace.cPanelHdr : "transparent")
                                border.color: sel ? workspace.cAccentDim : workspace.cBorder
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: parent.modelData
                                    color: parent.sel ? workspace.cAccent : workspace.cTextDim
                                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.2
                                }
                                MouseArea {
                                    id: tabMA; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: clusterMamPanel.leftTab = index
                                }
                            }
                        }
                    }

                    // Cluster (tab 0)
                    Loader {
                        anchors { top: clusterMamTabs.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; topMargin: 4 }
                        active: workspace.visible
                        visible: clusterMamPanel.leftTab === 0
                        sourceComponent: clusterComp
                    }
                    Component {
                        id: clusterComp
                        DxClusterPanel {
                            embedded: true   // 1.0.343 — no drag/resize interni nel workspace
                            minPanelWidth: 0
                            minPanelHeight: 0
                            x: 0; y: 0
                            width: parent ? parent.width : 320
                            height: parent ? parent.height : 280
                            radius: 0
                            border.width: 0
                        }
                    }

                    // MAM (tab 1) — 1.0.345 contenuto MAM incassato (MamPanel).
                    Loader {
                        anchors { top: clusterMamTabs.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; topMargin: 4 }
                        active: workspace.visible && clusterMamPanel.leftTab === 1
                        visible: clusterMamPanel.leftTab === 1
                        sourceComponent: mamPanelComp
                    }
                    Component {
                        id: mamPanelComp
                        MamPanel {
                            anchors.fill: parent
                            engine: workspace.engine
                        }
                    }
                }

                DxPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 100
                    title: "PSK Reporter"
                    meta: "heard-by"
                    live: true
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible
                        sourceComponent: pskReporterComp
                    }
                    Component {
                        id: pskReporterComp
                        PSKReporterPanel { anchors.fill: parent }
                    }
                }
            }

            // ---- COL CENTER : Waterfall / Full Spectrum ----------------------
            SplitView {
                id: colCenterSplit
                orientation: Qt.Vertical
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxPanel {
                    SplitView.preferredHeight: 340
                    SplitView.minimumHeight: 160
                    title: "Waterfall"
                    meta: "panadapter"
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible
                        sourceComponent: waterfallComp
                    }
                    Component {
                        id: waterfallComp
                        Waterfall {
                            anchors.fill: parent
                            showControls: true
                        }
                    }
                }

                DxPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 120
                    title: "Full Spectrum · Decode"
                    meta: "live"
                    live: true
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible
                        sourceComponent: fullSpectrumComp
                    }
                    Component {
                        id: fullSpectrumComp
                        FullSpectrumPanel { anchors.fill: parent }
                    }
                }
            }

            // ---- COL RIGHT : Signal RX / TX / Log ----------------------------
            SplitView {
                id: colRightSplit
                orientation: Qt.Vertical
                SplitView.preferredWidth: 520
                SplitView.minimumWidth: 380
                handle: Rectangle {
                    implicitWidth: 8; implicitHeight: 8
                    color: SplitHandle.pressed ? workspace.cAccent
                         : (SplitHandle.hovered ? workspace.cAccentDim : "transparent")
                    Rectangle { anchors.centerIn: parent; width: 28; height: 2; radius: 1; color: workspace.cBorder }
                }

                DxPanel {
                    SplitView.preferredHeight: 350
                    SplitView.minimumHeight: 160
                    title: "Signal RX · QSO Lock"
                    meta: "live"
                    live: true
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible
                        sourceComponent: signalRxComp
                    }
                    Component {
                        id: signalRxComp
                        SignalRxPanel { anchors.fill: parent }
                    }
                }

                DxPanel {
                    SplitView.preferredHeight: 250
                    SplitView.minimumHeight: 150
                    title: "TX Macros"
                    meta: "live"
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible && workspace.engine !== undefined && workspace.engine !== null
                        sourceComponent: txComp
                    }
                    Component {
                        id: txComp
                        TxPanel {
                            anchors.fill: parent
                            engine: workspace.engine
                            handleLogPrompt: false
                            showAsyncIcon: false
                            showBandBar: false
                            // 1.0.344 — il pulsante MAM in TX Macros apre la finestra MAM
                            // (propagata a Main.qml che possiede l'istanza MamWindow).
                            onMamWindowRequested: workspace.requestOpenMam()
                        }
                    }
                }

                DxPanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 100
                    title: "Log · QSO Entry"
                    meta: "live"
                    Loader {
                        anchors.fill: parent
                        active: workspace.visible
                        sourceComponent: logQsoComp
                    }
                    Component {
                        id: logQsoComp
                        LogQsoPanel { anchors.fill: parent }
                    }
                }
            }
        }

        // -------- ROW 4 — STATUS BAR (28px) ------------------------------------
        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            audioLevel: workspace.bridge ? workspace.bridge.audioLevel : 0.0
            signalLevel: workspace.bridge ? workspace.bridge.sMeter : 0.0
            monitoring: workspace.bridge ? workspace.bridge.monitoring : false
            transmitting: workspace.bridge ? workspace.bridge.transmitting : false
            tuning: workspace.bridge ? workspace.bridge.tuning : false
            decoding: workspace.bridge ? workspace.bridge.decoding : false
            catStatus: (workspace.bridge && workspace.bridge.catConnected) ? "Connected" : "Disconnected"
        }
    }
}
