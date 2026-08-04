/* Decodium Qt6 - TX Panel Component
 * TX panel delegates to bridge (which uses HvTxW for auto-sequencing)
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../../panels"

Item {
    id: txPanel
    implicitHeight: panelColumn.implicitHeight + 8

    // 1.0.262 — CALL feature: signal emesso quando l'utente clicca il pulsante CALL
    signal callRequested()
    signal ft2LinkAccessRequested()

    // Required property - reference to the app engine
    required property var engine
    property var ft2LinkToolPanel: null
    property bool handleLogPrompt: true
    property bool showAsyncIcon: true
    // 1.0.342 — nascondi la band-bar interna quando un'altra (es. DX-Pedition ROW2) e' gia' presente.
    property bool showBandBar: true

    // 1.0.308 (#4 fix) — filtro "bande operative" nel band-bar REALE. BandSelector.qml era
    // codice morto (non istanziato): il selettore effettivo è questo (bandSelectorBar).
    // Le bande deselezionate in Settings>Display (setting uiDisabledBands, CSV di lambda)
    // vengono nascoste. Reattivo via onSettingValueChanged.
    property string bandDisabledCsv: engine ? String(engine.getSetting("uiDisabledBands", "") || "") : ""
    function bandIsEnabled(lambda) {
        if (!bandDisabledCsv || bandDisabledCsv.length === 0) return true
        return ("," + bandDisabledCsv + ",").indexOf("," + lambda + ",") < 0
    }
    Connections {
        target: engine
        function onSettingValueChanged(key, value) {
            if (key === "uiDisabledBands") txPanel.bandDisabledCsv = String(value || "")
        }
    }
    readonly property bool txVisualActive: !!(engine && (engine.transmitting || engine.tuning))
    property string logPreviewCall: ""
    property string logPreviewGrid: ""
    property string logPreviewSent: ""
    property string logPreviewRcvd: ""
    property string logPreviewFreq: ""
    property string logPreviewMode: ""
    property string logPreviewTimeOn: ""
    property string logPreviewTimeOff: ""
    property string logPreviewComment: ""
    property bool logClusterSpotAvailable: false
    property bool logClusterSpotChecked: false
    property bool logPromptRequestedByBridge: false
    property bool logPromptAccepted: false
    property var logSatelliteChoices: [""]
    property var logSatModeChoices: [""]
    property int editingTxNum: 0
    property string editingTxError: ""
    readonly property real compactTxButtonWidth: Math.max(104, Math.min(220, (width - 56) / 6))

    function refreshLogPreview() {
        var preview = engine && engine.pendingLogQsoPreview ? engine.pendingLogQsoPreview() : ({})
        logPreviewCall = preview.call ? String(preview.call) : (engine && engine.dxCall ? engine.dxCall : "")
        logPreviewGrid = preview.grid ? String(preview.grid) : (engine && engine.dxGrid ? engine.dxGrid : "")
        logPreviewSent = preview.sent ? String(preview.sent) : (engine && engine.reportSent ? engine.reportSent : "")
        logPreviewRcvd = preview.rcvd ? String(preview.rcvd) : (engine && engine.reportReceived ? engine.reportReceived : "")
        logPreviewFreq = preview.freq !== undefined ? Number(preview.freq || 0).toFixed(0)
                                                    : (engine ? Number(engine.frequency || 0).toFixed(0) : "")
        logPreviewMode = preview.mode ? String(preview.mode) : (engine && engine.mode ? engine.mode : "")
        logPreviewTimeOn = preview.timeOn !== undefined ? String(preview.timeOn || "") : ""
        logPreviewTimeOff = preview.timeOff !== undefined ? String(preview.timeOff || "") : ""
        logPreviewComment = preview.comment !== undefined ? String(preview.comment || "") : ""
    }

    function refreshLogSatelliteChoices() {
        logSatelliteChoices = engine ? engine.satelliteOptions() : [""]
        logSatModeChoices = engine ? engine.satModeOptions() : [""]
    }

    function satelliteCodeFromDisplay(displayText) {
        var text = String(displayText || "").trim()
        if (text.length === 0)
            return ""
        var separator = text.indexOf(" - ")
        return separator > 0 ? text.substring(0, separator).trim() : text
    }

    function satelliteDisplayForCode(code) {
        var target = String(code || "").trim()
        if (target.length === 0)
            return ""
        for (var i = 0; i < logSatelliteChoices.length; ++i) {
            var candidate = String(logSatelliteChoices[i] || "")
            if (satelliteCodeFromDisplay(candidate) === target)
                return candidate
        }
        return ""
    }

    function setComboText(combo, text) {
        var idx = combo.find(String(text || ""))
        combo.currentIndex = idx >= 0 ? idx : 0
    }

    function syncLogSatelliteFields() {
        refreshLogSatelliteChoices()
        if (!engine)
            return
        setComboText(satelliteCombo, satelliteDisplayForCode(engine.getSetting("Satellite", "")))
        setComboText(satModeCombo, engine.getSetting("SatMode", ""))
    }

    function openTxMessageEditor(txNum, currentMessage) {
        if (!engine)
            return
        editingTxNum = txNum
        editingTxError = ""
        txEditField.text = String(currentMessage || "")
        txEditPopup.open()
        txEditField.forceActiveFocus()
        txEditField.selectAll()
    }

    function applyTxMessageEdit(sendAfter) {
        if (!engine || editingTxNum < 1 || editingTxNum > 6)
            return
        var msg = String(txEditField.text || "").trim().toUpperCase()
        var validation = engine.validateTxMessage ? engine.validateTxMessage(msg) : ""
        if (validation && validation.length > 0) {
            editingTxError = validation
            return
        }
        if (engine.setTxMessage)
            engine.setTxMessage(editingTxNum, msg)
        else
            engine["tx" + editingTxNum] = msg
        if (sendAfter)
            engine.sendTx(editingTxNum)
        txEditPopup.close()
    }

    function openLogPromptFromBridge() {
        // The docked and popped TxPanel instances coexist. Only the visible,
        // active panel may own the prompt for the current QSO.
        if (!engine || !handleLogPrompt || !txPanel.visible)
            return
        var hostWindow = txPanel.Window.window
        if (hostWindow) {
            if (!hostWindow.visible)
                return
            if (hostWindow.visibility === Window.Minimized && hostWindow.showNormal)
                hostWindow.showNormal()
            else if (hostWindow.show)
                hostWindow.show()
            if (hostWindow.raise)
                hostWindow.raise()
            if (hostWindow.requestActivate)
                hostWindow.requestActivate()
        }
        logPromptRequestedByBridge = true
        logPromptAccepted = false
        refreshLogPreview()
        syncLogSatelliteFields()
        logCommentField.text = logPreviewComment
        logGridField.text = logPreviewGrid
        logTimeOnField.text = logPreviewTimeOn
        logTimeOffField.text = logPreviewTimeOff
        logConfirmPopup.open()
    }

    // Convenience aliases
    property var log: engine ? engine.logManager : null

    Connections {
        target: engine
        ignoreUnknownSignals: true
        function onLogQsoPromptRequested() {
            txPanel.openLogPromptFromBridge()
        }
    }

    // Signal for MAM window request
    signal mamWindowRequested()

    // ── Ordine pulsanti TX panel (drag&drop magnetico, persistente) ──────────
    // Replica della meccanica STEP 1 (toolbar superiore in Main.qml): modello-
    // ordine separato + Repeater + Component-per-pulsante + Loader + MouseArea
    // unica con holdTimer (click-vs-drag) + ghost proxy + snap magnetico.
    // Il Mode selector NON è nel modello (resta primo elemento fisso).
    // 1.0.388 — aggiunto "cqfilter" (selettore livello filtro CQ) accanto a "tune", movibile come gli altri.
    readonly property string uiTxPanelOrderDefault: "mam,deep,ap,seq,quickqso,enabletx,holdfreq,autocq,call,txphase,alt12,halt,clear,tune,cqfilter,async,hound"
    readonly property var uiTxPanelKnownIds: ["mam","deep","ap","seq","quickqso","enabletx","holdfreq","autocq","call","txphase","alt12","halt","clear","tune","cqfilter","async","hound"]
    property var uiTxPanelOrder: parseTxPanelOrder(String(bridge.getSetting("uiTxPanelOrder", "") || ""))

    // Parsa il CSV salvato in una lista di id; ripristina il default se assente/corrotto.
    function parseTxPanelOrder(csv) {
        var def = uiTxPanelOrderDefault.split(",")
        if (!csv || csv.length === 0)
            return def
        var parts = String(csv).split(",")
        var out = []
        var seen = ({})
        for (var i = 0; i < parts.length; ++i) {
            var id = parts[i].trim()
            if (id.length === 0)
                continue
            if (uiTxPanelKnownIds.indexOf(id) < 0)
                continue            // id sconosciuto -> scarta
            if (seen[id])
                continue            // dedup
            seen[id] = true
            out.push(id)
        }
        // Aggiungi eventuali id mancanti (nuovo pulsante introdotto dopo) in coda.
        for (var j = 0; j < def.length; ++j) {
            if (!seen[def[j]]) {
                out.push(def[j])
                seen[def[j]] = true
            }
        }
        if (out.length === 0)
            return def
        return out
    }

    function persistTxPanelOrder() {
        if (bridge) bridge.setSetting("uiTxPanelOrder", uiTxPanelOrder.join(","))
    }

    // Sposta l'id dalla posizione 'from' alla posizione 'to' nel modello e committa.
    function moveTxPanelButton(from, to) {
        if (from === to || from < 0 || to < 0)
            return
        var arr = uiTxPanelOrder.slice()
        if (from >= arr.length || to >= arr.length)
            return
        var item = arr.splice(from, 1)[0]
        arr.splice(to, 0, item)
        uiTxPanelOrder = arr
        persistTxPanelOrder()
    }

    // === Regolazione livello TX (drive/Pwr) con rotellina + tasto destro su "Enable TX" ===
    // bridge.txOutputLevel 0..450 = attenuazione 0..45 dB (0 = massima uscita). La % mostrata
    // e' "potenza": 100% = 0 dB att (level 0), 0% = 45 dB att (level 450).
    readonly property real txPwrStepLevel: 9.0   // 2% di potenza per tacca di rotellina
    function txPwrPercent() {
        if (!bridge) return 0
        return Math.round((450.0 - bridge.txOutputLevel) / 4.5)
    }
    function nudgeTxPwr(up) {
        if (!bridge) return
        var nv = bridge.txOutputLevel + (up ? -txPwrStepLevel : txPwrStepLevel)
        if (nv < 0) nv = 0
        if (nv > 450) nv = 450
        bridge.txOutputLevel = nv
        txPwrOverlay.flash()
    }

    // Overlay temporaneo: mostra la potenza TX durante la regolazione con la rotellina.
    Rectangle {
        id: txPwrOverlay
        z: 9999
        visible: opacity > 0.01
        opacity: 0.0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 6
        width: txPwrCol.width + 28
        height: txPwrCol.height + 16
        radius: 8
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
        border.color: errorRed
        border.width: 1
        Behavior on opacity { NumberAnimation { duration: 180 } }
        function flash() { opacity = 1.0; txPwrHideTimer.restart() }
        Timer { id: txPwrHideTimer; interval: 1100; onTriggered: txPwrOverlay.opacity = 0.0 }
        Column {
            id: txPwrCol
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("TX Power")
                color: textSecondary
                font.pixelSize: 10
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: txPanel.txPwrPercent() + "%"
                color: errorRed
                font.pixelSize: 20
                font.bold: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: (bridge ? (bridge.txOutputLevel/10.0).toFixed(1) : "0") + " dB att"
                color: textSecondary
                font.pixelSize: 9
            }
        }
    }

    // Reattivo al reset dal tab "UI Buttons" di SettingsDialog (setting svuotato).
    Connections {
        target: engine
        ignoreUnknownSignals: true
        function onSettingValueChanged(key, value) {
            if (key === "uiTxPanelOrder")
                txPanel.uiTxPanelOrder = txPanel.parseTxPanelOrder(String(value || ""))
        }
    }

    // Style properties
    property color glassBg: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.85)
    property color glassBorder: bridge.themeManager.glassBorder
    property color primaryBlue: bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color successGreen: bridge.themeManager.accentColor
    property color warningOrange: bridge.themeManager.warningColor
    property color errorRed: bridge.themeManager.errorColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color bgDeep: bridge.themeManager.bgDeep
    readonly property var supportedModes: engine ? engine.availableModes() : ["FT8", "FT2", "FT2-Link", "FT4", "Q65", "MSK144", "JT65", "JT9", "JT4", "FST4", "FST4W", "WSPR"]
    readonly property real toolbarScale: Math.max(0.9, Math.min(1.12, bridge ? bridge.fontScale : 1.0))
    readonly property int toolbarSpacing: showBandBar ? 3 : 2
    readonly property int toolbarButtonHeight: 32
    readonly property int toolbarButtonHPad: Math.max(11, Math.round(12 * toolbarScale))
    readonly property int toolbarMinButtonWidth: showBandBar ? Math.max(54, Math.round(56 * toolbarScale)) : 44
    readonly property int toolbarMaxButtonWidth: showBandBar ? Math.max(98, Math.round(104 * toolbarScale)) : 86
    readonly property int toolbarButtonWidth: Math.max(54, Math.round(58 * toolbarScale))
    readonly property int toolbarLongButtonWidth: Math.max(toolbarButtonWidth,
                                                           Math.round(70 * toolbarScale))
    readonly property int toolbarHoldButtonWidth: toolbarButtonWidth
    readonly property int toolbarWideButtonWidth: toolbarButtonWidth
    readonly property int toolbarModeWidth: isFt2LinkMode
                                            ? Math.max(132, toolbarMeasuredWidth("FT2-Link", "\u25BE") + 24)
                                            : toolbarMeasuredWidth("FT2-Link", "\u25BE")
    readonly property int toolbarLabelSize: Math.max(9, Math.round(9 * toolbarScale))
    readonly property int toolbarGlyphSize: Math.max(12, Math.round(13 * toolbarScale))
    readonly property int qsoInfoControlHeight: 30
    readonly property int qsoInfoLabelSize: Math.max(10, Math.round(10 * toolbarScale))
    readonly property int qsoInfoFieldSize: Math.max(11, Math.round(11 * toolbarScale))
    readonly property int qsoInfoPad: Math.max(8, Math.round(9 * toolbarScale))
    readonly property int toolbarControlsWidth: toolbarModeWidth
                                                + (toolbarButtonWidth * 7)
                                                + toolbarButtonWidth
                                                + (toolbarButtonWidth * 6)
                                                + (toolbarSpacing * 15)
    readonly property int toolbarBandWidth: Math.max(220, Math.min(520, topControlsFlow.width))

    function toolbarMeasuredWidth(label, glyph) {
        var text = String(label || "")
        var icon = String(glyph || "")
        var charPx = Math.max(6.2, 6.8 * toolbarScale)
        var iconPx = icon.length > 0 ? toolbarGlyphSize + 4 : 0
        return Math.max(toolbarMinButtonWidth,
                        Math.min(toolbarMaxButtonWidth,
                                 Math.round(text.length * charPx + iconPx + toolbarButtonHPad * 2)))
    }

    function toolbarActionWidth(label, glyph) {
        // Keep action controls visually consistent. Only labels longer than
        // five characters need the wider variant; glyph/emoji metrics must
        // not resize neighbouring buttons from platform to platform.
        return String(label || "").length > 5
                ? toolbarLongButtonWidth
                : toolbarButtonWidth
    }

    function displayModeName(mode) {
        var text = String(mode || "").trim()
        var upper = text.toUpperCase()
        if (upper === "FT2-LINK" || upper === "FT2LINK" || upper === "D4LINK" || upper === "D4 LINK")
            return "FT2-Link"
        return text.length > 0 ? text : "FT8"
    }

    readonly property bool isFt2LinkMode: displayModeName(engine ? engine.mode : "") === "FT2-Link"
    readonly property string normalizedOperatingMode: String(engine ? engine.mode : "").trim().toUpperCase()
    readonly property bool isWsprBeaconMode: normalizedOperatingMode === "WSPR"
                                               || normalizedOperatingMode === "FST4W"
                                               || normalizedOperatingMode.indexOf("FST4W-") === 0
    readonly property var ft2LinkHiddenTxPanelIds: ({
        "mam": true,
        "deep": true,
        "ap": true,
        "seq": true,
        "quickqso": true,
        "enabletx": true,
        "holdfreq": true,
        "autocq": true,
        "call": true,
        "txphase": true,
        "alt12": true,
        "clear": true,
        "cqfilter": true,
        "async": true,
        "hound": true
    })

    function txPanelButtonVisible(buttonId, defaultVisible) {
        if (txPanel.isFt2LinkMode && txPanel.ft2LinkHiddenTxPanelIds[String(buttonId || "")])
            return false
        return !!defaultVisible
    }

    function syncModeSelector() {
        if (typeof modeSelector === "undefined" || !modeSelector)
            return
        var mode = displayModeName(engine ? engine.mode : "FT8")
        var idx = modeSelector.model.indexOf(mode)
        modeSelector.currentIndex = idx >= 0 ? idx : 0
    }

    function wsprPowerDbmFromLabel(label) {
        var parsed = parseInt(String(label || ""), 10)
        return isNaN(parsed) ? 37 : parsed
    }

    function wsprPowerIndexFor(dbm, model) {
        var needle = String(Number(dbm || 37)) + " dBm"
        for (var i = 0; model && i < model.length; ++i) {
            if (String(model[i]).indexOf(needle) === 0)
                return i
        }
        return 11
    }

    function qsoInfoWidth(text, placeholder, minWidth, maxWidth) {
        var value = String(text || "")
        var hint = String(placeholder || "")
        var chars = Math.max(value.length, hint.length)
        var charPx = Math.max(6.5, 7.1 * toolbarScale)
        return Math.max(minWidth,
                        Math.min(maxWidth,
                                 Math.round(chars * charPx + qsoInfoPad * 2)))
    }

    function bandButtonWidth(label) {
        var text = String(label || "")
        if (text.indexOf("cm") >= 0)
            return text.length > 5 ? 66 : 56
        if (text.indexOf(".") >= 0)
            return 58
        return Math.max(38, Math.min(68, text.length * 8 + 16))
    }

    // State colors based on QSO progress (bridge::QSOProgress enum)
    // 0=IDLE, 1=CALLING_CQ, 2=REPLYING, 3=REPORT, 4=ROGER_REPORT, 5=SIGNOFF, 6=IDLE_QSO
    property color stateColor: {
        if (!engine) return textSecondary
        switch (engine.qsoProgress) {
            case 0: return textSecondary      // IDLE
            case 1: return accentGreen        // CALLING_CQ
            case 2: return primaryBlue        // REPLYING
            case 3: return secondaryCyan      // REPORT
            case 4: return warningOrange      // ROGER_REPORT
            case 5: return warningOrange      // SIGNOFF
            case 6: return accentGreen        // IDLE_QSO (QSO completed)
            default: return textSecondary
        }
    }

    property string stateText: {
        if (!engine) return "Not Ready"
        switch (engine.qsoProgress) {
            case 0: return "Idle"
            case 1: return "Calling CQ"
            case 2: return "Replying"
            case 3: return "Sending Report"
            case 4: return "Roger Report"
            case 5: return "Sending 73"
            case 6: return "QSO Done"
            default: return "Unknown"
        }
    }

    component ToolbarButtonContent: Item {
        required property string label
        property string glyph: ""
        property color foreground: "white"
        property int glyphSize: 14
        property int labelSize: 10
        property bool boldLabel: false
        implicitWidth: contentLayout.implicitWidth
        implicitHeight: contentLayout.implicitHeight
        clip: true

        RowLayout {
            id: contentLayout
            anchors.fill: parent
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            spacing: glyph.length > 0 ? 1 : 0

            Text {
                visible: glyph.length > 0
                Layout.preferredWidth: Math.max(glyphSize, 12)
                Layout.fillHeight: true
                text: glyph
                color: foreground
                font.pixelSize: glyphSize
                font.family: decodiumMonoFontFamily
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: label
                color: foreground
                font.pixelSize: labelSize
                font.family: decodiumMonoFontFamily
                font.bold: boldLabel
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }

    Rectangle {
        id: panelFrame
        anchors.fill: parent
        color: glassBg
        border.color: txPanel.txVisualActive ? errorRed : glassBorder
        border.width: txPanel.txVisualActive ? 2 : 1
        radius: 12

        ColumnLayout {
            id: panelColumn
            anchors.fill: parent
            anchors.margins: 4
            spacing: 3


            // ===== Band + Mode toggles row =====
            ColumnLayout {
                id: topControlsRow
                Layout.fillWidth: true
                spacing: 3

                Rectangle {
                    id: bandSelectorBar
                    visible: txPanel.showBandBar
                    Layout.fillWidth: true
                    Layout.rightMargin: txPanel.showAsyncIcon ? 36 : 0
                    Layout.preferredHeight: visible ? 28 : 0
                    Layout.minimumHeight: visible ? 28 : 0
                    radius: 4
                    color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.9)
                    border.color: glassBorder
                    clip: true

                    Flickable {
                        id: bandButtonsFlickable
                        anchors.fill: parent
                        anchors.margins: 1
                        contentWidth: Math.max(bandButtonsRow.implicitWidth, bandButtonsRow.childrenRect.width)
                        contentHeight: Math.max(bandButtonsRow.implicitHeight, bandButtonsRow.childrenRect.height)
                        flickableDirection: Flickable.HorizontalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: contentWidth > width
                        clip: true

                        Row {
                            id: bandButtonsRow
                            spacing: 1

                            Repeater {
                                model: [
                                    { label: "160", lambda: "160M" },
                                    { label: "80", lambda: "80M" },
                                    { label: "60", lambda: "60M" },
                                    { label: "40", lambda: "40M" },
                                    { label: "30", lambda: "30M" },
                                    { label: "20", lambda: "20M" },
                                    { label: "17", lambda: "17M" },
                                    { label: "15", lambda: "15M" },
                                    { label: "12", lambda: "12M" },
                                    { label: "10", lambda: "10M" },
                                    { label: "8", lambda: "8M" },
                                    { label: "6", lambda: "6M" },
                                    { label: "4", lambda: "4M" },
                                    { label: "2", lambda: "2M" },
                                    { label: "1.25m", lambda: "1.25M" },
                                    { label: "70cm", lambda: "70CM" },
                                    { label: "33cm", lambda: "33CM" },
                                    { label: "23cm", lambda: "23CM" },
                                    { label: "13cm", lambda: "13CM" },
                                    { label: "9cm", lambda: "9CM" },
                                    { label: "6cm", lambda: "6CM" },
                                    { label: "3cm", lambda: "3CM" },
                                    { label: "1.25cm", lambda: "1.25CM" }
                                ]

                                Rectangle {
                                    id: bandRect
                                    readonly property string bandLabel: modelData.label
                                    readonly property string bandLambda: modelData.lambda

                                    visible: txPanel.bandIsEnabled(bandLambda)  // 1.0.308 (#4 fix)

                                    width: txPanel.bandButtonWidth(bandLabel)
                                    height: 26
                                    radius: 3

                                    readonly property bool isSelected: engine && engine.bandManager &&
                                                                       engine.bandManager.currentBandLambda === bandLambda

                                    color: isSelected ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.5) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                    border.color: isSelected ? secondaryCyan : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.2)
                                    border.width: isSelected ? 2 : 1

                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        text: bandRect.bandLabel
                                        font.family: decodiumMonoFontFamily
                                        font.pixelSize: 10
                                        font.bold: bandRect.isSelected
                                        color: bandRect.isSelected ? "#ffffff" : textPrimary
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        hoverEnabled: true
                                        onClicked: {
                                            if (engine && engine.bandManager)
                                                engine.bandManager.changeBandByLambda(bandRect.bandLambda)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.rightMargin: txPanel.showAsyncIcon ? 36 : 0
                    Layout.preferredHeight: topControlsFlow.implicitHeight
                    Layout.minimumHeight: topControlsFlow.implicitHeight
                    implicitHeight: topControlsFlow.implicitHeight

                    Flow {
                        id: topControlsFlow
                        width: parent.width
                        height: topControlsFlow.implicitHeight
                        spacing: txPanel.toolbarSpacing

                        Rectangle {
                            width: txPanel.toolbarModeWidth
                            height: txPanel.toolbarButtonHeight
                            radius: 5
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.15)
                            border.color: secondaryCyan
                            border.width: 1

                            StyledComboBox {
                                id: modeSelector
                                anchors.fill: parent
                                anchors.margins: 1
                                model: txPanel.supportedModes
                                currentIndex: 0
                                Component.onCompleted: txPanel.syncModeSelector()
                                onActivated: function(index) {
                                    if (!engine || index < 0 || index >= model.length)
                                        return
                                    var selectedMode = String(model[index] || "")
                                    if (selectedMode.toUpperCase() === "FT2-LINK"
                                            && !engine.ft2LinkAccessUnlocked) {
                                        txPanel.ft2LinkAccessRequested()
                                        Qt.callLater(txPanel.syncModeSelector)
                                        return
                                    }
                                    if (selectedMode.length > 0)
                                        engine.mode = selectedMode
                                }
                                Connections {
                                    target: engine
                                    function onModeChanged() {
                                        txPanel.syncModeSelector()
                                    }
                                    function onFt2LinkAccessChanged() {
                                        txPanel.syncModeSelector()
                                    }
                                }
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: Math.max(11, Math.round(11 * txPanel.toolbarScale))
                                itemHeight: 34
                                popupMinWidth: 176
                                textHorizontalAlignment: Text.AlignHCenter
                                leftPadding: 4
                                rightPadding: 18
                                topPadding: 4
                                bottomPadding: 4
                                bgColor: "transparent"
                                borderColor: "transparent"
                            }
                        }

                        Rectangle {
                            visible: engine && txPanel.isWsprBeaconMode
                            width: Math.max(94, Math.round(108 * txPanel.toolbarScale))
                            height: txPanel.toolbarButtonHeight
                            radius: 5
                            color: Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.14)
                            border.color: accentGreen
                            border.width: 1

                            StyledComboBox {
                                id: wsprPowerSelector
                                anchors.fill: parent
                                anchors.margins: 1
                                model: engine ? engine.wsprPowerOptions() : []
                                currentIndex: txPanel.wsprPowerIndexFor(engine ? engine.wsprPowerDbm : 37, model)
                                onActivated: function(index) {
                                    if (!engine || index < 0 || index >= model.length)
                                        return
                                    engine.wsprPowerDbm = txPanel.wsprPowerDbmFromLabel(model[index])
                                }
                                Connections {
                                    target: engine
                                    function onWsprPowerDbmChanged() {
                                        if (wsprPowerSelector)
                                            wsprPowerSelector.currentIndex = txPanel.wsprPowerIndexFor(engine.wsprPowerDbm, wsprPowerSelector.model)
                                    }
                                }
                                font.family: decodiumMonoFontFamily
                                font.pixelSize: Math.max(10, Math.round(10 * txPanel.toolbarScale))
                                itemHeight: 34
                                popupMinWidth: 150
                                textHorizontalAlignment: Text.AlignHCenter
                                leftPadding: 4
                                rightPadding: 18
                                topPadding: 4
                                bottomPadding: 4
                                bgColor: "transparent"
                                borderColor: "transparent"
                            }
                        }

                        FT2LinkTabBar {
                            id: ft2LinkToolTabs
                            visible: txPanel.isFt2LinkMode && txPanel.ft2LinkToolPanel !== null
                            panel: txPanel.ft2LinkToolPanel
                            readonly property real availableWidth: Math.max(260,
                                                                            topControlsFlow.width
                                                                            - txPanel.toolbarModeWidth
                                                                            - (txPanel.toolbarButtonWidth * 2)
                                                                            - (txPanel.toolbarSpacing * 6)
                                                                            - 16)
                            width: visible
                                   ? Math.max(260,
                                              Math.min(Math.max(260, implicitWidth),
                                                       availableWidth))
                                   : 0
                            height: visible ? implicitHeight : 0
                        }

                        // ── Pulsanti TX panel riordinabili via drag&drop magnetico ──
                        // Replica STEP 1 (toolbar in Main.qml): un Repeater itera il modello
                        // ORDINATO (txPanel.uiTxPanelOrder) e un Loader per slot carica il
                        // Component giusto. Click breve = azione; long-press = drag (riordino).
                        // Contratto Component: btnVisible, prefWidth, hovered, tip, activate(mouse).
                        Repeater {
                            id: txCtrlRepeater
                            model: txPanel.uiTxPanelOrder

                            function componentForId(id) {
                                switch (id) {
                                    case "mam":       return comp_mam
                                    case "deep":      return comp_deep
                                    case "ap":        return comp_ap
                                    case "seq":       return comp_seq
                                    case "quickqso":  return comp_quickqso
                                    case "enabletx":  return comp_enabletx
                                    case "holdfreq":  return comp_holdfreq
                                    case "autocq":    return comp_autocq
                                    case "call":      return comp_call
                                    case "txphase":   return comp_txphase
                                    case "alt12":     return comp_alt12
                                    case "halt":      return comp_halt
                                    case "clear":     return comp_clear
                                    case "tune":      return comp_tune
                                    case "cqfilter":  return comp_cqfilter
                                    case "async":     return comp_async
                                    case "hound":     return comp_hound
                                    default:          return null
                                }
                            }

                            // Stato drag condiviso fra gli slot
                            property int dragIndex: -1
                            property int dropIndex: -1

                            // Indice modello di destinazione in base alla X (scena) del puntatore.
                            function computeTargetIndex(from, sceneX) {
                                var n = txCtrlRepeater.count
                                var target = from
                                for (var i = 0; i < n; ++i) {
                                    if (i === from)
                                        continue
                                    var it = txCtrlRepeater.itemAt(i)
                                    if (!it || !it.visible)
                                        continue
                                    var mid = it.x + it.width / 2
                                    if (i < from && sceneX < mid) { target = i; break }
                                    if (i > from) {
                                        if (sceneX > mid) target = i
                                    }
                                }
                                return target
                            }

                            // Ogni slot e' un Item dimensionato come il pulsante (Flow lo posiziona
                            // in base a width/height; visible:false -> Flow salta lo slot).
                            delegate: Item {
                                id: slot
                                property string buttonId: modelData
                                property bool slotVisible: btnLoader.item ? txPanel.txPanelButtonVisible(buttonId, btnLoader.item.btnVisible) : false
                                property bool dragging: txCtrlRepeater.dragIndex === index

                                visible: slotVisible
                                width: btnLoader.item ? btnLoader.item.prefWidth : 0
                                height: txPanel.toolbarButtonHeight
                                z: dragging ? 10 : 0

                                Loader {
                                    id: btnLoader
                                    anchors.fill: parent
                                    sourceComponent: txCtrlRepeater.componentForId(slot.buttonId)
                                    opacity: slot.dragging ? 0.25 : 1.0
                                    Behavior on opacity { NumberAnimation { duration: 120 } }
                                    // Spostamento magnetico: gli slot fra origine e destinazione
                                    // scorrono per "aprire" il varco; animato (NO layer.enabled/FBO).
                                    x: {
                                        var d = txCtrlRepeater.dragIndex
                                        var t = txCtrlRepeater.dropIndex
                                        if (d < 0 || t < 0 || index === d)
                                            return 0
                                        var dragged = txCtrlRepeater.itemAt(d)
                                        var shift = (dragged ? dragged.width : 0) + topControlsFlow.spacing
                                        if (t > d && index > d && index <= t)
                                            return -shift
                                        if (t < d && index >= t && index < d)
                                            return shift
                                        return 0
                                    }
                                    Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                                    onLoaded: {
                                        if (item)
                                            item.hovered = Qt.binding(function() { return dragMA.containsMouse && txCtrlRepeater.dragIndex < 0 })
                                    }
                                }

                                // MouseArea unica: hover + click + long-press->drag.
                                MouseArea {
                                    id: dragMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    preventStealing: true

                                    property bool armed: false
                                    property real pressSceneX: 0
                                    property bool moved: false
                                    property bool wheelAdjusted: false

                                    Timer {
                                        id: holdTimer
                                        interval: 350
                                        repeat: false
                                        onTriggered: {
                                            if (dragMA.pressedButtons & Qt.LeftButton) {
                                                dragMA.armed = true
                                                txCtrlRepeater.dragIndex = index
                                                txCtrlRepeater.dropIndex = index
                                                txDragGhost.startFor(slot, index)
                                            }
                                        }
                                    }

                                    onPressed: function(mouse) {
                                        armed = false
                                        moved = false
                                        wheelAdjusted = false
                                        pressSceneX = mapToItem(topControlsFlow, mouse.x, mouse.y).x
                                        if (mouse.button === Qt.LeftButton)
                                            holdTimer.start()
                                    }

                                    onPositionChanged: function(mouse) {
                                        var sx = mapToItem(topControlsFlow, mouse.x, mouse.y).x
                                        if (Math.abs(sx - pressSceneX) > 6)
                                            moved = true
                                        if (armed) {
                                            txDragGhost.updateX(sx)
                                            // NON muta il modello durante il drag (eviterebbe la
                                            // distruzione del delegate) -> commit al rilascio.
                                            txCtrlRepeater.dropIndex = txCtrlRepeater.computeTargetIndex(index, sx)
                                        } else if (moved) {
                                            holdTimer.stop()
                                        }
                                    }

                                    onReleased: function(mouse) {
                                        holdTimer.stop()
                                        if (armed) {
                                            var sx = mapToItem(topControlsFlow, mouse.x, mouse.y).x
                                            var target = txCtrlRepeater.computeTargetIndex(index, sx)
                                            txDragGhost.stop()
                                            txCtrlRepeater.dragIndex = -1
                                            txCtrlRepeater.dropIndex = -1
                                            armed = false
                                            if (target !== index)
                                                txPanel.moveTxPanelButton(index, target)
                                            return
                                        }
                                        // Regolazione con rotellina (destro tenuto): nessuna azione al rilascio.
                                        if (wheelAdjusted) { wheelAdjusted = false; return }
                                        // Sul pulsante con wheelAdjustsTx il destro e' dedicato alla regolazione: niente toggle.
                                        if (mouse.button === Qt.RightButton && btnLoader.item && btnLoader.item.wheelAdjustsTx === true)
                                            return
                                        // Click breve: esegui azione (passa mouse per right-click MAM).
                                        if (!moved && btnLoader.item)
                                            btnLoader.item.activate(mouse)
                                    }

                                    onCanceled: {
                                        holdTimer.stop()
                                        if (armed) {
                                            txDragGhost.stop()
                                            txCtrlRepeater.dragIndex = -1
                                            txCtrlRepeater.dropIndex = -1
                                            armed = false
                                        }
                                    }

                                    // Tasto destro tenuto + rotellina su un pulsante con
                                    // wheelAdjustsTx (Enable TX): regola la potenza TX (txOutputLevel).
                                    onWheel: function(wheel) {
                                        if (btnLoader.item && btnLoader.item.wheelAdjustsTx === true
                                                && (wheel.buttons & Qt.RightButton)) {
                                            txPanel.nudgeTxPwr(wheel.angleDelta.y > 0)
                                            wheelAdjusted = true
                                            wheel.accepted = true
                                        } else {
                                            wheel.accepted = false
                                        }
                                    }

                                    ToolTip.visible: containsMouse && txCtrlRepeater.dragIndex < 0 && btnLoader.item && btnLoader.item.tip.length > 0
                                    ToolTip.text: btnLoader.item ? btnLoader.item.tip : ""
                                    ToolTip.delay: 500
                                }
                            }
                        }

                        // ════════ Component dei pulsanti TX panel ════════
                        // Ogni Component conserva aspetto e logica originali del pulsante.
                        // Contratto: btnVisible (gate), prefWidth (larghezza nel Flow),
                        // hovered (impostato dallo slot/ghost), tip (ToolTip), activate(mouse).

                        // MAM (Rectangle wrapper; sinistro=toggle, destro=apri finestra MAM)
                        Component {
                            id: comp_mam
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("MAM", "⇆")
                                readonly property string tip: qsTr("Multi-Answer Mode (MAM) - right-click opens the window (default OFF)")
                                function activate(mouse) {
                                    if (mouse && mouse.button === Qt.RightButton)
                                        txPanel.mamWindowRequested()
                                    else if (engine)
                                        engine.multiAnswerMode = !engine.multiAnswerMode
                                }
                                radius: 5
                                color: (engine && engine.multiAnswerMode) ? Qt.rgba(255/255, 152/255, 0, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: (engine && engine.multiAnswerMode) ? warningOrange : (hovered ? secondaryCyan : glassBorder)
                                border.width: (engine && engine.multiAnswerMode) ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "MAM"
                                    glyph: "⇆"
                                    foreground: (engine && engine.multiAnswerMode) ? warningOrange : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: engine && engine.multiAnswerMode
                                }
                            }
                        }

                        // DEEP
                        Component {
                            id: comp_deep
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("DEEP", "◎")
                                readonly property string tip: qsTr("Deep Search: deeper weak-signal search using known callsigns (default OFF)")
                                function activate(mouse) { if (engine) engine.deepSearchEnabled = !engine.deepSearchEnabled }
                                radius: 5
                                color: (engine && engine.deepSearchEnabled) ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: (engine && engine.deepSearchEnabled) ? accentGreen : (hovered ? secondaryCyan : glassBorder)
                                border.width: (engine && engine.deepSearchEnabled) ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "DEEP"
                                    glyph: "◎"
                                    foreground: (engine && engine.deepSearchEnabled) ? accentGreen : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: engine && engine.deepSearchEnabled
                                }
                            }
                        }

                        // AP
                        Component {
                            id: comp_ap
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("AP", "◆")
                                readonly property string tip: qsTr("A priori decode (AP): uses known information to recover weak signals (default OFF)")
                                function activate(mouse) { if (engine) engine.ft8ApEnabled = !engine.ft8ApEnabled }
                                radius: 5
                                color: (engine && engine.ft8ApEnabled) ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: (engine && engine.ft8ApEnabled) ? secondaryCyan : (hovered ? secondaryCyan : glassBorder)
                                border.width: (engine && engine.ft8ApEnabled) ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "AP"
                                    glyph: "◆"
                                    foreground: (engine && engine.ft8ApEnabled) ? secondaryCyan : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: engine && engine.ft8ApEnabled
                                }
                            }
                        }

                        // SEQ
                        Component {
                            id: comp_seq
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("SEQ", "↻")
                                readonly property string tip: qsTr("Automatic QSO sequencing (default OFF)")
                                function activate(mouse) { if (engine) engine.autoSeq = !engine.autoSeq }
                                radius: 5
                                color: (engine && engine.autoSeq) ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: (engine && engine.autoSeq) ? primaryBlue : (hovered ? secondaryCyan : glassBorder)
                                border.width: (engine && engine.autoSeq) ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "SEQ"
                                    glyph: "↻"
                                    foreground: (engine && engine.autoSeq) ? primaryBlue : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: engine && engine.autoSeq
                                }
                            }
                        }

                        // Quick QSO
                        Component {
                            id: comp_quickqso
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("QQ", "⇨")
                                readonly property string tip: qsTr("Quick QSO: skips TX1 and starts from TX2 (direct report) (default OFF)")
                                function activate(mouse) { if (engine) engine.quickQsoEnabled = !engine.quickQsoEnabled }
                                radius: 5
                                color: (engine && engine.quickQsoEnabled) ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: (engine && engine.quickQsoEnabled) ? accentGreen : (hovered ? secondaryCyan : glassBorder)
                                border.width: (engine && engine.quickQsoEnabled) ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "QQ"
                                    glyph: "⇨"
                                    foreground: (engine && engine.quickQsoEnabled) ? accentGreen : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: engine && engine.quickQsoEnabled
                                }
                            }
                        }

                        // Enable TX
                        Component {
                            id: comp_enabletx
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("TX", "▲")
                                readonly property bool txActive: engine ? engine.txEnabled : false
                                readonly property bool wheelAdjustsTx: true
                                readonly property string tip: qsTr("Enable TX\nRight button + mouse wheel: adjust TX power")
                                function activate(mouse) {
                                    if (!engine) return
                                    if (!engine.txEnabled) {
                                        engine.txEnabled = true
                                    } else if (engine.autoCqRepeat) {
                                        return
                                    } else {
                                        engine.haltWithReason("qml-tx-enable-toggle")
                                    }
                                }
                                radius: 5
                                color: txActive ? Qt.rgba(244/255, 67/255, 54/255, 0.2) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: txActive ? errorRed : (hovered ? secondaryCyan : glassBorder)
                                border.width: txActive ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "TX"
                                    glyph: "▲"
                                    foreground: parent.txActive ? errorRed : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: true
                                }
                            }
                        }

                        // Hold Tx Freq (bind a bridge.holdTxFreq)
                        Component {
                            id: comp_holdfreq
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("HOLD", "🔓")
                                readonly property bool isHeld: bridge ? bridge.holdTxFreq : false
                                readonly property string tip: qsTr("Lock the TX frequency\n(Hold Tx Freq)")
                                function activate(mouse) { if (bridge) bridge.holdTxFreq = !bridge.holdTxFreq }
                                radius: 5
                                color: isHeld ? Qt.rgba(255/255, 193/255, 7/255, 0.25) : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: isHeld ? "#FFC107" : (hovered ? secondaryCyan : glassBorder)
                                border.width: isHeld ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "HOLD"
                                    glyph: parent.isHeld ? "🔒" : "🔓"
                                    foreground: parent.isHeld ? "#FFC107" : textSecondary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: parent.isHeld
                                }
                            }
                        }

                        // Auto CQ
                        Component {
                            id: comp_autocq
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("ACQ", "⟳")
                                readonly property bool acqOn: engine && engine.autoCqRepeat
                                readonly property string tip: qsTr("Repeated Auto CQ\nAutomatically calls CQ until a reply arrives (default OFF)")
                                function activate(mouse) {
                                    if (engine) {
                                        if (!engine.autoSeq && !engine.autoCqRepeat)
                                            engine.autoSeq = true
                                        engine.autoCqRepeat = !engine.autoCqRepeat
                                    }
                                }
                                radius: 5
                                color: acqOn ? Qt.alpha(successGreen, 0.3) : Qt.alpha(textPrimary, 0.05)
                                border.color: acqOn ? successGreen : (hovered ? secondaryCyan : Qt.alpha(textPrimary, 0.2))
                                border.width: acqOn ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "ACQ"
                                    glyph: "⟳"
                                    foreground: parent.acqOn ? successGreen : textPrimary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: parent.acqOn
                                }
                            }
                        }

                        // CALL
                        Component {
                            id: comp_call
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("CALL", "📞")
                                readonly property bool callOn: engine && engine.targetCallActive
                                readonly property string tip: engine && engine.targetCallActive
                                              ? qsTr("Active call: %1 (missed target %2/%3)\nClick to open the panel").arg(engine.targetCallSign)
                                                    .arg(engine.targetCallRetryCount)
                                                    .arg(engine.targetCallMaxRetries === 0 ? "∞" : engine.targetCallMaxRetries)
                                              : qsTr("Direct call (CALL)\nOpen the direct callsign call panel\nwith missed-target limit, timeout, and period control")
                                function activate(mouse) { txPanel.callRequested() }
                                radius: 5
                                color: callOn ? Qt.alpha(successGreen, 0.3) : Qt.alpha(textPrimary, 0.05)
                                border.color: callOn ? successGreen : (hovered ? secondaryCyan : Qt.alpha(textPrimary, 0.2))
                                border.width: callOn ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "CALL"
                                    glyph: "📞"
                                    foreground: parent.callOn ? successGreen : textPrimary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: parent.callOn
                                }
                            }
                        }

                        // TX Phase (gate: mode !== FT2)
                        Component {
                            id: comp_txphase
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: engine && engine.mode !== "FT2"
                                readonly property bool first: engine && engine.txPeriod === 1
                                readonly property real prefWidth: txPanel.toolbarActionWidth(first ? "1ST" : "2ND", first ? "①" : "②")
                                readonly property string tip: qsTr("TX slot\n1st: :00/:30\n2nd: :15/:45")
                                function activate(mouse) { if (engine) engine.txPeriod = engine.txPeriod === 1 ? 0 : 1 }
                                radius: 5
                                color: first ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.28)
                                             : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: first ? primaryBlue : (hovered ? secondaryCyan : glassBorder)
                                border.width: first ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: parent.first ? "1ST" : "2ND"
                                    glyph: parent.first ? "①" : "②"
                                    foreground: parent.first ? primaryBlue : textPrimary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: true
                                }
                            }
                        }

                        // ALT 1/2 (gate: mode !== FT2)
                        Component {
                            id: comp_alt12
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: engine && engine.mode !== "FT2"
                                readonly property bool altOn: engine && engine.alt12Enabled
                                readonly property real prefWidth: txPanel.toolbarActionWidth("ALT 1/2", "⇄")
                                readonly property string tip: qsTr("Auto CQ: alternates TX/RX phases after repeated unanswered CQs (default OFF)")
                                function activate(mouse) { if (engine) engine.alt12Enabled = !engine.alt12Enabled }
                                radius: 5
                                color: altOn ? Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.22)
                                             : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                                border.color: altOn ? warningOrange : (hovered ? secondaryCyan : glassBorder)
                                border.width: altOn ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "ALT 1/2"
                                    glyph: "⇄"
                                    foreground: parent.altOn ? warningOrange : textPrimary
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: parent.altOn
                                }
                            }
                        }

                        // HALT
                        Component {
                            id: comp_halt
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("HALT", "■")
                                readonly property string tip: qsTr("Stop TX")
                                function activate(mouse) { if (engine) engine.haltWithReason("qml-halt-button") }
                                radius: 5
                                color: txPanel.txVisualActive
                                       ? Qt.rgba(errorRed.r, errorRed.g, errorRed.b, hovered ? 0.96 : 0.88)
                                       : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, hovered ? 0.28 : 0.16)
                                border.color: txPanel.txVisualActive ? Qt.lighter(errorRed, 1.2) : accentGreen
                                border.width: txPanel.txVisualActive ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "HALT"
                                    glyph: "■"
                                    foreground: txPanel.txVisualActive ? "#FFF8F6" : accentGreen
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: true
                                }
                            }
                        }

                        // CLEAR (niente glyph)
                        Component {
                            id: comp_clear
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property real prefWidth: txPanel.toolbarActionWidth("CLEAR", "")
                                readonly property string tip: qsTr("Clear DX, reports, and TX1-TX5")
                                function activate(mouse) { if (engine) engine.clearTxMessages() }
                                radius: 5
                                color: hovered ? Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.24)
                                               : Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.12)
                                border.color: warningOrange
                                border.width: 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: "CLEAR"
                                    foreground: warningOrange
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: true
                                }
                            }
                        }

                        // TUNE
                        Component {
                            id: comp_tune
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property bool isTuning: engine && engine.tuning
                                readonly property real prefWidth: txPanel.toolbarActionWidth(isTuning ? "STOP" : "TUNE", "♫")
                                readonly property string tip: qsTr("Tune (transmits the tuning carrier)")
                                function activate(mouse) {
                                    if (engine) {
                                        if (engine.tuning) engine.stopTune()
                                        else engine.startTune()
                                    }
                                }
                                radius: 5
                                color: isTuning ? Qt.alpha(warningOrange, 0.5) : Qt.alpha(warningOrange, 0.2)
                                border.color: warningOrange
                                border.width: isTuning ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: parent.isTuning ? "STOP" : "TUNE"
                                    glyph: "♫"
                                    foreground: warningOrange
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: true
                                }
                            }
                        }

                        // CQ filter cycler (1.0.388): OFF → CQ → CQ·73 → CQ·RR73 → CQ·RRR → OFF.
                        // Gestisce sia filterCqOnly (on/off) sia cqFilterLevel (0..3).
                        Component {
                            id: comp_cqfilter
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: true
                                readonly property bool filterOn: engine && engine.filterCqOnly
                                readonly property int lvl: engine ? engine.cqFilterLevel : 0
                                readonly property string lbl: "CQ" + (filterOn ? ["", "·73", "·RR73", "·RRR"][lvl] : "")
                                readonly property real prefWidth: txPanel.toolbarActionWidth(lbl, "▾")
                                readonly property string tip: filterOn
                                    ? qsTr("CQ filter active (%1). Click: change level / turn off.")
                                          .arg([qsTr("CQ only"), "CQ+73", "CQ+73+RR73", "CQ+73+RR73+RRR"][lvl])
                                    : qsTr("CQ filter off. Click: show only CQ, then cycle 73 / RR73 / RRR.")
                                function activate(mouse) {
                                    if (!engine) return
                                    if (!engine.filterCqOnly) { engine.filterCqOnly = true; engine.cqFilterLevel = 0 }
                                    else if (engine.cqFilterLevel < 3) { engine.cqFilterLevel = engine.cqFilterLevel + 1 }
                                    else { engine.filterCqOnly = false }
                                }
                                radius: 5
                                color: filterOn ? Qt.alpha(secondaryCyan, 0.4)
                                                : (hovered ? Qt.alpha(secondaryCyan, 0.18) : Qt.alpha(secondaryCyan, 0.08))
                                border.color: filterOn ? secondaryCyan
                                              : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, hovered ? 1.0 : 0.4)
                                border.width: filterOn ? 2 : 1
                                ToolbarButtonContent {
                                    anchors.fill: parent
                                    label: parent.lbl
                                    glyph: "▾"
                                    foreground: secondaryCyan
                                    glyphSize: txPanel.toolbarGlyphSize
                                    labelSize: txPanel.toolbarLabelSize
                                    boldLabel: parent.filterOn
                                }
                            }
                        }

                        // Async (AsyncModeWidget; display-only, gate: FT2 + showAsyncIcon)
                        Component {
                            id: comp_async
                            AsyncModeWidget {
                                // 'hovered' è già definito da AsyncModeWidget: lo riusiamo come da contratto.
                                readonly property bool btnVisible: txPanel.showAsyncIcon && engine && engine.mode === "FT2"
                                readonly property real prefWidth: 90
                                readonly property string tip: qsTr("Async FT2 mode - sine wave: green=RX, red=TX (default OFF)")
                                function activate(mouse) {}
                                running: btnVisible && engine && (engine.asyncTxEnabled || engine.asyncDecodeEnabled)
                                transmitting: engine ? engine.transmitting : false
                                snr: engine ? engine.asyncSnrDb : -99
                            }
                        }

                        // HOUND badge (display-only, gate: houndMode)
                        Component {
                            id: comp_hound
                            Rectangle {
                                property bool hovered: false
                                readonly property bool btnVisible: engine && engine.houndMode
                                readonly property real prefWidth: 82
                                readonly property string tip: qsTr("Hound mode active")
                                function activate(mouse) {}
                                radius: 5
                                color: Qt.rgba(warningOrange.r, warningOrange.g, warningOrange.b, 0.24)
                                border.color: warningOrange
                                border.width: 2
                                Text {
                                    anchors.centerIn: parent
                                    text: "HOUND"
                                    color: warningOrange
                                    font.family: decodiumMonoFontFamily
                                    font.pixelSize: Math.max(11, Math.round(11 * txPanel.toolbarScale))
                                    font.bold: true
                                }
                            }
                        }

                    }

                    // ── Ghost trascinato (proxy visuale che segue il puntatore) ──
                    // Sibling del Flow (NON figlio: Flow non gestisce la sua x); overlay
                    // assoluto attivo SOLO durante il drag. Le coordinate combaciano col
                    // Flow perché entrambi hanno origine (0,0) in questo Item wrapper.
                    Item {
                        id: txDragGhost
                        visible: false
                        x: 0
                        y: 0
                        height: txPanel.toolbarButtonHeight
                        z: 50
                        property Item sourceSlot: null

                        function startFor(s, idx) {
                            sourceSlot = s
                            width = s.width
                            txGhostLoader.sourceComponent = txCtrlRepeater.componentForId(s.buttonId)
                            visible = true
                        }
                        function updateX(sceneX) {
                            var nx = sceneX - width / 2
                            if (nx < 0) nx = 0
                            if (nx > topControlsFlow.width - width) nx = topControlsFlow.width - width
                            x = nx
                        }
                        function stop() {
                            visible = false
                            sourceSlot = null
                            txGhostLoader.sourceComponent = null
                        }

                        Behavior on x { NumberAnimation { duration: 60; easing.type: Easing.OutQuad } }

                        Loader {
                            id: txGhostLoader
                            anchors.fill: parent
                            opacity: 0.9
                            onLoaded: { if (item) item.hovered = true }
                        }
                    }
                }
            }

            // DX Station info row
            RowLayout {
                id: classicQsoInfoRow
                visible: !txPanel.isFt2LinkMode
                enabled: visible
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.minimumHeight: visible ? implicitHeight : 0
                Layout.maximumHeight: visible ? implicitHeight : 0
                spacing: 5

                // QSO State indicator
                Rectangle {
                    Layout.preferredWidth: qsoInfoWidth(stateText, "Idle", 58, 118)
                    Layout.preferredHeight: qsoInfoControlHeight
                    color: Qt.alpha(stateColor, 0.2)
                    border.color: stateColor
                    radius: 4

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: qsoInfoPad
                        anchors.rightMargin: qsoInfoPad
                        text: stateText
                        color: stateColor
                        font.pixelSize: qsoInfoLabelSize
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: "DX:"
                    color: secondaryCyan
                    font.pixelSize: qsoInfoLabelSize
                    font.bold: true
                }

                DecoTextField {
                    id: dxCallField
                    Layout.preferredWidth: qsoInfoWidth(text, "Call", 74, 126)
                    Layout.preferredHeight: qsoInfoControlHeight
                    text: engine ? engine.dxCall : ""
                    placeholderText: "Call"
                    font.pixelSize: qsoInfoFieldSize
                    font.family: decodiumMonoFontFamily
                    color: textPrimary
                    leftPadding: qsoInfoPad
                    rightPadding: qsoInfoPad
                    topPadding: 0
                    bottomPadding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: dxCallField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.dxCall) {
                            engine.dxCall = text.toUpperCase()
                        }
                    }
                }

                Text {
                    text: "Grid:"
                    color: secondaryCyan
                    font.pixelSize: qsoInfoLabelSize
                    font.bold: true
                }

                DecoTextField {
                    id: dxGridField
                    Layout.preferredWidth: qsoInfoWidth(text, "Grid", 58, 84)
                    Layout.preferredHeight: qsoInfoControlHeight
                    text: engine ? engine.dxGrid : ""
                    placeholderText: "Grid"
                    font.pixelSize: qsoInfoFieldSize
                    font.family: decodiumMonoFontFamily
                    color: textPrimary
                    leftPadding: qsoInfoPad
                    rightPadding: qsoInfoPad
                    topPadding: 0
                    bottomPadding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: dxGridField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.dxGrid) {
                            engine.dxGrid = text.toUpperCase()
                        }
                    }
                }

                Text {
                    text: "S:"
                    color: secondaryCyan
                    font.pixelSize: qsoInfoLabelSize
                    font.bold: true
                }

                DecoTextField {
                    id: rptSentField
                    Layout.preferredWidth: qsoInfoWidth(text, "-10", 44, 58)
                    Layout.preferredHeight: qsoInfoControlHeight
                    text: engine ? engine.reportSent : "-10"
                    font.pixelSize: qsoInfoFieldSize
                    font.family: decodiumMonoFontFamily
                    color: textPrimary
                    leftPadding: 4
                    rightPadding: 4
                    topPadding: 0
                    bottomPadding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: rptSentField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.reportSent) {
                            engine.reportSent = text
                        }
                    }
                }

                Text {
                    text: "R:"
                    color: secondaryCyan
                    font.pixelSize: qsoInfoLabelSize
                    font.bold: true
                }

                DecoTextField {
                    id: rptRcvdField
                    Layout.preferredWidth: qsoInfoWidth(text, "--", 44, 58)
                    Layout.preferredHeight: qsoInfoControlHeight
                    text: engine ? engine.reportReceived : ""
                    placeholderText: "--"
                    font.pixelSize: qsoInfoFieldSize
                    font.family: decodiumMonoFontFamily
                    color: accentGreen
                    leftPadding: 4
                    rightPadding: 4
                    topPadding: 0
                    bottomPadding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter

                    background: Rectangle {
                        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.8)
                        border.color: rptRcvdField.focus ? primaryBlue : glassBorder
                        radius: 3
                    }

                    onTextChanged: {
                        if (engine && text !== engine.reportReceived) {
                            engine.reportReceived = text
                        }
                    }
                }

                // Next/TX Message display
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: qsoInfoControlHeight
                    color: engine && engine.transmitting ?
                           Qt.alpha(errorRed, 0.2) :
                           Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.6)
                    border.color: engine && engine.transmitting ? errorRed : glassBorder
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 4

                        Text {
                            text: engine && engine.transmitting ? "TX:" : "Next:"
                            color: engine && engine.transmitting ? errorRed : textSecondary
                            font.pixelSize: 10
                            font.bold: engine && engine.transmitting
                        }

                        Text {
                            Layout.fillWidth: true
                            text: engine ? engine.currentTxMessage : ""
                            color: engine && engine.transmitting ? errorRed : textPrimary
                            font.family: decodiumMonoFontFamily
                            font.pixelSize: 11
                            font.bold: engine && engine.transmitting
                            elide: Text.ElideRight
                        }

                    }
                }

                // Log QSO button
                Button {
                    id: logQsoBtn
                    Layout.preferredWidth: qsoInfoWidth("LOG", "\u270E", 58, 70)
                    Layout.preferredHeight: qsoInfoControlHeight
                    enabled: engine && engine.dxCall.length > 0 && !txPanel.txVisualActive
                    padding: 0
                    topInset: 0
                    bottomInset: 0
                    leftInset: 0
                    rightInset: 0

                    background: Rectangle {
                        color: logQsoBtn.enabled ?
                               Qt.alpha(accentGreen, 0.3) :
                               Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
                        border.color: logQsoBtn.enabled ? accentGreen : glassBorder
                        radius: 4
                    }

                    contentItem: Item {
                        implicitWidth: logButtonContent.implicitWidth
                        implicitHeight: logButtonContent.implicitHeight

                        Row {
                            id: logButtonContent
                            spacing: 5
                            anchors.centerIn: parent

                            Text {
                                text: "\u270E"
                                color: logQsoBtn.enabled ? accentGreen : textSecondary
                                font.pixelSize: 14
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "LOG"
                                color: logQsoBtn.enabled ? accentGreen : textSecondary
                                font.pixelSize: 11
                                font.bold: true
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    onClicked: {
                        if (!engine) {
                            return
                        }
                        if (engine.requestManualLogQso)
                            engine.requestManualLogQso()
                        else if (engine.promptLogQso)
                            engine.promptLogQso()
                        else
                            engine.logQso()
                    }

                    ToolTip.visible: logQsoBtn.hovered
                    ToolTip.delay: 500
                    ToolTip.text: txPanel.txVisualActive
                                  ? qsTr("Finish TX before logging the QSO")
                                  : (engine && engine.dxCall.length > 0
                                     ? qsTr("Log current QSO")
                                     : qsTr("No active QSO to log"))
                }
            }

            // TX Buttons row
            RowLayout {
                id: classicTxButtonsRow
                visible: !txPanel.isFt2LinkMode
                enabled: visible
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? implicitHeight : 0
                Layout.minimumHeight: visible ? implicitHeight : 0
                Layout.maximumHeight: visible ? implicitHeight : 0
                spacing: 6

                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

                // TX1 - Answer CQ (Grid)
                TxButton {
                    txNum: 1
                    label: "TX1"
                    message: engine && engine.txMessages.length > 0 ? engine.txMessages[0] : "-- --"
                    isSelected: engine && engine.currentTx === 1
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 1
                    // 1.0.132: skip mask blocca anche left-click (era solo auto-seq)
                    onClicked: if (engine && !isDisabled) engine.sendTx(1)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                // TX2 - Send Report
                TxButton {
                    txNum: 2
                    label: "TX2"
                    message: engine && engine.txMessages.length > 1 ? engine.txMessages[1] : "-- --"
                    isSelected: engine && engine.currentTx === 2
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 2
                    onClicked: if (engine && !isDisabled) engine.sendTx(2)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                // TX3 - R+Report
                TxButton {
                    txNum: 3
                    label: "TX3"
                    message: engine && engine.txMessages.length > 2 ? engine.txMessages[2] : "-- --"
                    isSelected: engine && engine.currentTx === 3
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 3
                    onClicked: if (engine && !isDisabled) engine.sendTx(3)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                // TX4 - RR73
                TxButton {
                    txNum: 4
                    label: "TX4"
                    message: engine && engine.txMessages.length > 3 ? engine.txMessages[3] : "-- --"
                    isSelected: engine && engine.currentTx === 4
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 4
                    onClicked: if (engine && !isDisabled) engine.sendTx(4)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                // TX5 - 73
                TxButton {
                    txNum: 5
                    label: "TX5"
                    message: engine && engine.txMessages.length > 4 ? engine.txMessages[4] : "-- --"
                    isSelected: engine && engine.currentTx === 5
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 5
                    onClicked: if (engine && !isDisabled) engine.sendTx(5)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                // TX6 - CQ
                TxButton {
                    txNum: 6
                    label: "TX6"
                    message: engine && engine.txMessages.length > 5 ? engine.txMessages[5] : "-- --"
                    isSelected: engine && engine.currentTx === 6
                    isTransmitting: engine && engine.transmitting && engine.currentTx === 6
                    isCQ: true
                    onClicked: if (engine && !isDisabled) engine.sendTx(6)
                    onEditRequested: txPanel.openTxMessageEditor(txNum, message)
                }

                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                }

            }

        }

        Rectangle {
            id: txPulseOverlay
            anchors.fill: parent
            color: "transparent"
            radius: 12
            border.color: errorRed
            border.width: 3
            visible: txPanel.txVisualActive
            opacity: 0

            SequentialAnimation on opacity {
                running: txPanel.txVisualActive
                loops: Animation.Infinite
                NumberAnimation { to: 1.0; duration: 250 }
                NumberAnimation { to: 0.3; duration: 250 }
                onRunningChanged: if (!running) txPulseOverlay.opacity = 0
            }
        }
    }

    Popup {
        id: txEditPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        width: 520
        height: 220
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: parent ? Math.round((parent.height - height) / 2) : 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: 10
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
            border.color: secondaryCyan
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "TX" + txPanel.editingTxNum
                    color: secondaryCyan
                    font.pixelSize: 18
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    text: "✕"
                    onClicked: txEditPopup.close()
                }
            }

            DecoTextField {
                id: txEditField
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                color: textPrimary
                selectedTextColor: bgDeep
                selectionColor: secondaryCyan
                font.pixelSize: 16
                font.family: decodiumMonoFontFamily
                selectByMouse: true
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                onTextChanged: txPanel.editingTxError = ""
                onAccepted: txPanel.applyTxMessageEdit(false)
                background: Rectangle {
                    radius: 5
                    color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                    border.color: txPanel.editingTxError.length > 0 ? errorRed
                                                                     : (txEditField.activeFocus ? secondaryCyan : glassBorder)
                    border.width: 1
                }
            }

            Text {
                Layout.fillWidth: true
                text: txPanel.editingTxError
                visible: txPanel.editingTxError.length > 0
                color: errorRed
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    text: qsTr("Cancel")
                    onClicked: txEditPopup.close()
                }

                Button {
                    text: qsTr("Standard")
                    enabled: engine && engine.resetStandardTxMessages
                    onClicked: {
                        engine.resetStandardTxMessages()
                        txEditPopup.close()
                    }
                }

                Button {
                    text: qsTr("Apply")
                    onClicked: txPanel.applyTxMessageEdit(false)
                }

                Button {
                    text: "TX"
                    onClicked: txPanel.applyTxMessageEdit(true)
                }
            }
        }
    }

    Window {
        id: logConfirmPopup
        visible: false
        transientParent: txPanel.Window.window
        modality: Qt.ApplicationModal
        flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        title: qsTr("Confirm QSO logging")
        color: "transparent"
        width: 570
        height: 560

        function centerOnHostWindow() {
            var host = transientParent
            var targetX = host ? host.x + Math.round((host.width - width) / 2) : x
            var targetY = host ? host.y + Math.round((host.height - height) / 2) : y
            var available = null
            var centerX = host ? host.x + host.width / 2 : targetX + width / 2
            var centerY = host ? host.y + host.height / 2 : targetY + height / 2
            if (Qt.application && Qt.application.screens) {
                for (var i = 0; i < Qt.application.screens.length; ++i) {
                    var screenInfo = Qt.application.screens[i]
                    var geometry = screenInfo.availableGeometry
                    if (centerX >= geometry.x && centerX < geometry.x + geometry.width
                            && centerY >= geometry.y && centerY < geometry.y + geometry.height) {
                        available = geometry
                        break
                    }
                }
                if (!available && Qt.application.screens.length > 0)
                    available = Qt.application.screens[0].availableGeometry
            }
            if (available) {
                targetX = Math.max(available.x,
                                   Math.min(targetX, available.x + available.width - width))
                targetY = Math.max(available.y,
                                   Math.min(targetY, available.y + available.height - height))
            }
            x = Math.round(targetX)
            y = Math.round(targetY)
        }

        function open() {
            txPanel.refreshLogPreview()
            txPanel.syncLogSatelliteFields()
            logCommentField.text = txPanel.logPreviewComment
            logGridField.text = txPanel.logPreviewGrid
            logTimeOnField.text = txPanel.logPreviewTimeOn
            logTimeOffField.text = txPanel.logPreviewTimeOff
            txPanel.logClusterSpotAvailable = !!(engine && engine.dxCluster && engine.dxCluster.connected)
            txPanel.logClusterSpotChecked = txPanel.logClusterSpotAvailable && !!engine.autoSpotEnabled
            centerOnHostWindow()
            show()
            raise()
            requestActivate()
            // Dice al bridge che la finestra e' davvero comparsa: senza
            // questo lui non sa se la richiesta e' stata raccolta, e dopo
            // mezzo secondo apre il dialogo nativo di riserva.
            if (engine && engine.notifyLogPromptShown)
                engine.notifyLogPromptShown()
            Qt.callLater(function() {
                if (logGridField)
                    logGridField.forceActiveFocus()
            })
        }

        // Closing this application-modal native window marks the QSO as
        // skipped.  Unlike an in-scene Popup, it remains above detached
        // always-on-top traffic windows and cannot be hidden behind them.
        onVisibleChanged: {
            if (!visible) {
                if (txPanel.logPromptRequestedByBridge && !txPanel.logPromptAccepted
                        && engine && engine.rejectPromptedLogQso) {
                    engine.rejectPromptedLogQso()
                }
                txPanel.logPromptRequestedByBridge = false
                txPanel.logPromptAccepted = false
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
            border.color: accentGreen
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: qsTr("Confirm QSO logging")
                    color: accentGreen
                    font.pixelSize: 18
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    text: "✕"
                    onClicked: logConfirmPopup.close()
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: glassBorder }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8

                Text { text: "Call:"; color: textSecondary; font.pixelSize: 13 }
                Text { text: logPreviewCall || "-"; color: textPrimary; font.pixelSize: 14; font.bold: true }

                Text { text: "Grid:"; color: textSecondary; font.pixelSize: 13 }
                DecoTextField {
                    id: logGridField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: textPrimary
                    selectedTextColor: bgDeep
                    selectionColor: accentGreen
                    font.pixelSize: 14
                    selectByMouse: true
                    placeholderText: qsTr("Locator (e.g. JN71) — enter manually")
                    onTextEdited: {
                        var up = text.toUpperCase()
                        if (up !== text) { var p = cursorPosition; text = up; cursorPosition = p }
                    }
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                        border.color: logGridField.activeFocus ? accentGreen : glassBorder
                        border.width: 1
                    }
                }

                Text { text: "Report:"; color: textSecondary; font.pixelSize: 13 }
                Text { text: (logPreviewSent || "-") + " / " + (logPreviewRcvd || "-"); color: textPrimary; font.pixelSize: 14 }

                Text { text: "Mode:"; color: textSecondary; font.pixelSize: 13 }
                Text { text: logPreviewMode || "-"; color: textPrimary; font.pixelSize: 14 }

                Text { text: "Freq:"; color: textSecondary; font.pixelSize: 13 }
                Text { text: logPreviewFreq && logPreviewFreq !== "0" ? logPreviewFreq + " Hz" : "-"; color: textPrimary; font.pixelSize: 14 }

                Text { text: qsTr("Start UTC:"); color: textSecondary; font.pixelSize: 13 }
                DecoTextField {
                    id: logTimeOnField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: textPrimary
                    selectedTextColor: bgDeep
                    selectionColor: accentGreen
                    font.pixelSize: 13
                    font.family: decodiumMonoFontFamily
                    selectByMouse: true
                    placeholderText: "YYYY-MM-DD HH:MM:SS"
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                        border.color: logTimeOnField.activeFocus ? accentGreen : glassBorder
                        border.width: 1
                    }
                }

                Text { text: qsTr("End UTC:"); color: textSecondary; font.pixelSize: 13 }
                DecoTextField {
                    id: logTimeOffField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: textPrimary
                    selectedTextColor: bgDeep
                    selectionColor: accentGreen
                    font.pixelSize: 13
                    font.family: decodiumMonoFontFamily
                    selectByMouse: true
                    placeholderText: "YYYY-MM-DD HH:MM:SS"
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                        border.color: logTimeOffField.activeFocus ? accentGreen : glassBorder
                        border.width: 1
                    }
                }

                Text { text: "Comment:"; color: textSecondary; font.pixelSize: 13 }
                DecoTextField {
                    id: logCommentField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    color: textPrimary
                    selectedTextColor: bgDeep
                    selectionColor: accentGreen
                    font.pixelSize: 13
                    selectByMouse: true
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.08)
                        border.color: logCommentField.activeFocus ? accentGreen : glassBorder
                        border.width: 1
                    }
                }

                Text { text: "Satellite:"; color: textSecondary; font.pixelSize: 13 }
                StyledComboBox {
                    id: satelliteCombo
                    model: txPanel.logSatelliteChoices
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    font.pixelSize: 12
                    popupMinWidth: 320
                }

                Text { text: qsTr("Sat Mode:"); color: textSecondary; font.pixelSize: 13 }
                StyledComboBox {
                    id: satModeCombo
                    model: txPanel.logSatModeChoices
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    font.pixelSize: 12
                    popupMinWidth: 120
                    enabled: txPanel.satelliteCodeFromDisplay(satelliteCombo.currentText).length > 0
                }

                Text { text: qsTr("DX Cluster:"); color: textSecondary; font.pixelSize: 13 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    CheckBox {
                        id: logClusterSpotCheck
                        enabled: txPanel.logClusterSpotAvailable
                        checked: txPanel.logClusterSpotChecked
                        onCheckedChanged: txPanel.logClusterSpotChecked = checked
                    }

                    Text {
                        Layout.fillWidth: true
                        text: txPanel.logClusterSpotAvailable ? qsTr("Spot to cluster") : qsTr("Cluster not connected")
                        color: txPanel.logClusterSpotAvailable ? textPrimary : textSecondary
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 10

                Button {
                    id: logPromptRejectButton

                    text: qsTr("Skip")
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 36
                    padding: 0
                    onClicked: logConfirmPopup.close()
                    background: Rectangle {
                        radius: 6
                        color: logPromptRejectButton.down
                               ? Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.20)
                               : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.10)
                        border.color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, logPromptRejectButton.hovered ? 0.75 : 0.35)
                        border.width: 1
                    }
                    contentItem: Text {
                        text: logPromptRejectButton.text
                        color: textPrimary
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    id: logPromptAcceptButton

                    text: qsTr("Add")
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 36
                    padding: 0
                    enabled: logPreviewCall.length > 0
                    opacity: enabled ? 1.0 : 0.45
                    onClicked: {
                        if (engine) {
                            var satCode = txPanel.satelliteCodeFromDisplay(satelliteCombo.currentText)
                            var satMode = satCode.length > 0 ? String(satModeCombo.currentText || "").trim() : ""
                            engine.setSetting("Satellite", satCode)
                            engine.setSetting("SatMode", satMode)
                            engine.setSetting("PropMode", satCode.length > 0 ? "SAT" : "")
                            engine.setSetting("SaveSatellite", satCode.length > 0)
                            engine.setSetting("SaveSatMode", satCode.length > 0 && satMode.length > 0)
                            engine.setSetting("SavePropMode", satCode.length > 0)
                            if (engine.setNextLogComment)
                                engine.setNextLogComment(logCommentField.text)
                            else
                                engine.setSetting("LogComments", logCommentField.text)
                            if (engine.setNextLogGrid)  // 1.0.302: locator editabile a mano
                                engine.setNextLogGrid(logGridField.text)
                            if (engine.setNextLogTimes)
                                engine.setNextLogTimes(logTimeOnField.text, logTimeOffField.text)
                            engine.setNextLogClusterSpotEnabled(txPanel.logClusterSpotAvailable && txPanel.logClusterSpotChecked)
                            txPanel.logPromptAccepted = true
                            if (engine.confirmLogQso)
                                engine.confirmLogQso()
                            else
                                engine.logQso()
                        }
                        logConfirmPopup.close()
                    }
                    background: Rectangle {
                        radius: 6
                        color: logPromptAcceptButton.enabled
                               ? (logPromptAcceptButton.down
                                  ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.42)
                                  : Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, logPromptAcceptButton.hovered ? 0.32 : 0.22))
                               : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.07)
                        border.color: logPromptAcceptButton.enabled ? accentGreen : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.30)
                        border.width: 1
                    }
                    contentItem: Text {
                        text: logPromptAcceptButton.text
                        color: logPromptAcceptButton.enabled ? accentGreen : textSecondary
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // TX Button component
    component TxButton: Button {
        id: txButton
        property int txNum: 1
        property string label: "TX1"
        property string message: ""
        property bool isSelected: false
        property bool isTransmitting: false
        property bool isCQ: false
        // 1.0.130: bind a bridge.txDisabledMask per skip toggle
        readonly property bool isDisabled: bridge && (bridge.txDisabledMask & (1 << (txNum - 1))) !== 0
        signal editRequested(int txNum, string message)

        Layout.fillWidth: false
        Layout.preferredWidth: txPanel.compactTxButtonWidth
        Layout.maximumWidth: 220
        Layout.minimumWidth: 104
        Layout.preferredHeight: 44
        padding: 0
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        // fork: NON azzerare gli *Inset (li azzerava la "UI alignment" 1.0.399 di
        // Salvatore) -> ripristina l'elevation/ripple di Material cosi' i pulsanti TX
        // tornano a differire per stile UI (Material vs Fusion/Universal). Ri-applicare
        // se Salvatore ri-merge l'azzeramento degli inset.
        opacity: isDisabled ? 0.4 : 1.0

        background: Rectangle {
            color: {
                if (isDisabled) return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.04)
                if (isTransmitting) return Qt.alpha(errorRed, 0.4)
                if (isSelected) return Qt.alpha(primaryBlue, 0.3)
                if (isCQ) return Qt.alpha(accentGreen, 0.2)
                return Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.1)
            }
            border.color: {
                if (isDisabled) return Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.5)
                if (isTransmitting) return errorRed
                if (isSelected) return primaryBlue
                if (isCQ) return accentGreen
                return glassBorder
            }
            border.width: isSelected || isTransmitting ? 2 : 1
            radius: 7

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        contentItem: Item {
            implicitWidth: txPanel.compactTxButtonWidth
            implicitHeight: 44

            Column {
                anchors.centerIn: parent
                width: parent.width - 14
                spacing: 1

                Text {
                    width: parent.width
                    text: label
                    color: {
                        if (isTransmitting) return errorRed
                        if (isSelected) return primaryBlue
                        if (isCQ) return accentGreen
                        return textSecondary
                    }
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 10
                    font.bold: isSelected || isTransmitting
                    font.strikeout: txButton.isDisabled
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: message
                    color: isTransmitting ? errorRed : textPrimary
                    font.family: decodiumMonoFontFamily
                    font.pixelSize: 9
                    font.strikeout: txButton.isDisabled
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        ToolTip.visible: hovered
        ToolTip.delay: 800
        ToolTip.text: isDisabled
                          ? qsTr("TX%1 disabled (right-click -> menu to re-enable)").arg(txNum)
                          : qsTr("Click: send now\nRight-click: menu (Edit / Skip TX%1)\nLong-press: edit message").arg(txNum)

        // 1.0.302: clic destro apre il menu contestuale (Modifica messaggio + Salta TX),
        // su richiesta tester. L'edit resta disponibile anche via long-press.
        // Prima (1.0.130) il clic destro faceva solo skip diretto.
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: txButtonContextMenu.popup()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            longPressThreshold: 600
            onLongPressed: editRequested(txNum, message)
        }
        Menu {
            id: txButtonContextMenu
            MenuItem {
                text: qsTr("Edit TX%1 message").arg(txNum)
                height: 32
                onTriggered: editRequested(txNum, message)
                contentItem: Text { text: parent.text; color: textPrimary; font.pixelSize: 12; leftPadding: 10; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent" }
            }
            MenuItem {
                text: txButton.isDisabled ? qsTr("Re-enable TX%1").arg(txNum) : qsTr("Skip TX%1 (skip auto-seq)").arg(txNum)
                height: 32
                onTriggered: if (bridge) bridge.setTxDisabled(txNum, !txButton.isDisabled)
                contentItem: Text { text: parent.text; color: textPrimary; font.pixelSize: 12; leftPadding: 10; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.highlighted ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : "transparent" }
            }
            background: Rectangle {
                implicitWidth: 230
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
                border.color: glassBorder
                radius: 6
            }
        }
    }
}
