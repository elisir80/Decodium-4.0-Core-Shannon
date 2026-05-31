/* Waterfall — Decodium3 High-Resolution Panadapter
 * Uses PanadapterItem (QQuickItem + QSGNode, FFTW 4096-bin, SmartSDR style)
 * By IU8LMC — 2025
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// import Qt.labs.settings 1.1  // non disponibile in questa build Qt
import Decodium 1.0

Item {
    id: waterfallPanel

    signal frequencySelected(int freq)       // Destro = RX
    signal txFrequencySelected(int freq)     // Sinistro = TX

    property bool showControls: true
    property bool controlsExpanded: true
    property int  minFreq: 0
    property int  maxFreq: 3200
    property int  spectrumHeight: 150
    property bool restoringSettings: false
    property bool syncingPaletteChoice: false
    property bool showDecodeCallsigns: true
    property var spectrumDecodeLabels: []
    property bool dxClusterRefreshPending: false

    // Altezza minima/massima del grafico spettro (regolabile tramite drag)
    // 1.0.288 — vincoli rilassati: spettro e cascata ridimensionabili quasi liberamente
    // (decidere quale aprire di più). Spettro 40..(altezza-24), cascata ≥24.
    readonly property bool controlsVisible: showControls && controlsExpanded
    readonly property int spectrumMinHeight: 40
    readonly property int spectrumMaxHeight: 4000
    readonly property int waterfallMinHeight: controlsVisible ? 24 : 12

    // Colors
    property color bgDeep:      bridge.themeManager.bgDeep
    property color bgPanel:     Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.95)
    property color accentCyan:  bridge.themeManager.secondaryColor
    property color accentGreen: bridge.themeManager.accentColor
    property color textPrimary: bridge.themeManager.textPrimary
    property color textSec:     Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.6)
    property color borderColor: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.15)
    // Theme-aware toolbar palette (was hardcoded dark before).
    property color wfText:        bridge.themeManager.isLightTheme ? bridge.themeManager.textPrimary    : "#FFFFFF"
    property color wfToolbarBg:   bridge.themeManager.isLightTheme ? bridge.themeManager.panelHeader    : Qt.rgba(0,0,0,0.75)
    property color wfTrack:       bridge.themeManager.isLightTheme ? bridge.themeManager.borderSoft     : "#1a2a3a"
    property color wfFrame:       bridge.themeManager.isLightTheme ? bridge.themeManager.borderColor    : "#3a5470"
    property color wfYellow:      bridge.themeManager.isLightTheme ? bridge.themeManager.warningColor   : "#ffcc00"
    property color wfBlue:        bridge.themeManager.isLightTheme ? bridge.themeManager.primaryColor   : "#88BBDD"
    property color wfPurple:      bridge.themeManager.isLightTheme ? bridge.themeManager.ledMagenta     : "#AA88DD"
    property color wfSlate:       bridge.themeManager.isLightTheme ? bridge.themeManager.textSecondary  : "#5888AA"

    readonly property var labelColorPresets: [
        { name: "Auto",    color: "#00E6FF", custom: false },
        { name: "Cyan",    color: "#00E6FF", custom: true  },
        { name: "White",   color: "#FFFFFF", custom: true  },
        { name: "Yellow",  color: "#FFE600", custom: true  },
        { name: "Green",   color: "#00E664", custom: true  },
        { name: "Magenta", color: "#FF64FF", custom: true  },
        { name: "Orange",  color: "#FF9600", custom: true  }
    ]

    function applyManualContrast() {
        if (waterfallDisplay && !autoRangeCheck.checked) {
            waterfallDisplay.minDb = waterfallDisplay.measuredFloor - (150 - contrastSlider.value)
        }
    }

    function coerceBool(value, fallback) {
        if (value === true || value === false)
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off" || text.length === 0)
            return false
        return !!fallback
    }

    function boolSetting(key, fallback) {
        return coerceBool(bridge.getSetting(key, fallback), fallback)
    }

    function clampNumber(value, minValue, maxValue, fallback) {
        var n = Number(value)
        if (!isFinite(n))
            n = fallback
        return Math.max(minValue, Math.min(maxValue, n))
    }

    function clampIndex(value, count, fallback) {
        return Math.round(clampNumber(value, 0, Math.max(0, count - 1), fallback))
    }

    function clampPaletteIndex(value) {
        var count = 0
        if (paletteCombo && paletteCombo.count !== undefined)
            count = paletteCombo.count
        if ((!count || count < 1) && waterfallDisplay && waterfallDisplay.paletteNames)
            count = waterfallDisplay.paletteNames.length
        return clampIndex(value, Math.max(1, count), 0)
    }

    function scheduleGraphSave() {
        if (mainWindow && mainWindow.scheduleSave)
            mainWindow.scheduleSave()
    }

    function persistGraphSetting(key, value) {
        bridge.setSetting(key, value)
        scheduleGraphSave()
    }

    function setPaletteIndex(index, persist) {
        if (syncingPaletteChoice)
            return
        syncingPaletteChoice = true

        var next = clampPaletteIndex(index)
        if (paletteCombo.currentIndex !== next)
            paletteCombo.currentIndex = next
        if (!bridge.themeManager.isLightTheme && waterfallDisplay.paletteIndex !== next)
            waterfallDisplay.paletteIndex = next
        if (bridge.uiPaletteIndex !== next)
            bridge.uiPaletteIndex = next

        if (persist === undefined)
            persist = !waterfallPanel.restoringSettings
        if (persist)
            persistGraphSetting("uiPaletteIndex", next)

        syncingPaletteChoice = false
    }

    function loadPanadapterSettings() {
        restoringSettings = true
        waterfallPanel.controlsExpanded = boolSetting("uiWaterfallControlsExpanded", true)
        if (bridge.uiSpectrumHeight > 0) waterfallPanel.spectrumHeight = bridge.uiSpectrumHeight
        waterfallPanel.setPaletteIndex(bridge.getSetting("uiPaletteIndex", bridge.uiPaletteIndex), false)
        autoRangeCheck.checked = boolSetting("uiWaterfallAutoRange", true)
        txBracketsCheck.checked = boolSetting("uiWaterfallShowTxBrackets", true)
        peakHoldCheck.checked = boolSetting("uiWaterfallPeakHold", true)
        blackSlider.value = bridge.getSetting("uiWaterfallBlackLevel", 15)
        gainSlider.value = bridge.getSetting("uiWaterfallColorGain", 50)
        contrastSlider.value = bridge.getSetting("uiWaterfallContrast", 80)
        speedSlider.value = bridge.getSetting("spectrumInterval", 20)
        zoomSlider.value = bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0
        labelFontSlider.value = bridge.getSetting("uiLabelFontSize", 8)
        labelSpacingSlider.value = bridge.getSetting("uiLabelSpacing", 2)
        labelBoldCheck.checked = boolSetting("uiLabelBold", true)
        labelColorCombo.currentIndex = Math.max(0, Math.min(labelColorPresets.length - 1,
                                           bridge.getSetting("uiLabelColorPreset", 0)))
        waterfallPanel.setShowDecodeCallsigns(boolSetting("uiWaterfallShowCallsigns", true), false)
        dxClusterCheck.checked = boolSetting("uiWaterfallShowDxCluster", false)
        waterfallDisplay.showDxClusterSpots = dxClusterCheck.checked

        // In light theme la palette è forzata a 11 (mockup pastello). Non sovrascrivere col valore Settings.
        waterfallDisplay.autoRange = autoRangeCheck.checked
        waterfallDisplay.showTxBrackets = txBracketsCheck.checked
        waterfallDisplay.peakHold = peakHoldCheck.checked
        waterfallDisplay.blackLevel = blackSlider.value
        waterfallDisplay.colorGain = gainSlider.value
        waterfallDisplay.contrastLevel = contrastSlider.value
        waterfallDisplay.zoomFactor = zoomSlider.value
        waterfallDisplay.labelFontSize = labelFontSlider.value
        waterfallDisplay.labelSpacing = labelSpacingSlider.value
        waterfallDisplay.labelBold = labelBoldCheck.checked
        var preset = labelColorPresets[labelColorCombo.currentIndex]
        waterfallDisplay.labelUseCustomColor = preset.custom
        waterfallDisplay.labelColor = preset.color
        waterfallPanel.applyManualContrast()
        restoringSettings = false
    }

    function setControlsExpanded(expanded) {
        if (waterfallPanel.controlsExpanded === expanded)
            return
        waterfallPanel.controlsExpanded = expanded
        waterfallPanel.persistGraphSetting("uiWaterfallControlsExpanded", expanded)
    }

    function clearDecodeLabels() {
        decodeLabelSourceRefreshTimer.stop()
        spectrumDecodeLabels = []
        waterfallDisplay.setDecodeLabels([])
    }

    function setShowDecodeCallsigns(enabled, persist) {
        var next = coerceBool(enabled, true)
        var changed = waterfallPanel.showDecodeCallsigns !== next
        waterfallPanel.showDecodeCallsigns = next
        if (!next) {
            clearDecodeLabels()
        } else if (changed) {
            refreshDecodeLabels()
        }
        if (persist === undefined)
            persist = !waterfallPanel.restoringSettings
        if (persist) {
            waterfallPanel.persistGraphSetting("uiWaterfallShowCallsigns", next)
        }
    }

    function txSignalBandwidthHz(modeName) {
        var mode = String(modeName || bridge.mode || "").trim().toUpperCase()
        if (mode === "FT4")
            return 90
        if (mode === "FT8" || mode === "FT2")
            return 50
        return 0
    }

    Timer {
        id: decodeLabelSourceRefreshTimer
        interval: 160
        repeat: false
        onTriggered: waterfallPanel.refreshDecodeLabelsNow()
    }

    function refreshDecodeLabels(delayMs) {
        if (!waterfallPanel.visible)
            return
        if (!waterfallPanel.showDecodeCallsigns) {
            clearDecodeLabels()
            return
        }
        decodeLabelSourceRefreshTimer.interval = delayMs === undefined ? 160 : Math.max(0, delayMs)
        if (!decodeLabelSourceRefreshTimer.running)
            decodeLabelSourceRefreshTimer.start()
        else if (decodeLabelSourceRefreshTimer.interval === 0)
            decodeLabelSourceRefreshTimer.restart()
    }

    function refreshDecodeLabelsNow() {
        if (!waterfallPanel.visible)
            return
        if (!waterfallPanel.showDecodeCallsigns) {
            clearDecodeLabels()
            return
        }

        var labels = []
        var seen = {}
        var list = bridge.decodeList
        // Prendi solo gli ultimi decode (ultimo periodo) — max 30.
        // Scorri al contrario: se la stessa stazione compare piu' volte,
        // il waterfall deve mostrare il valore SNR del decode piu' recente,
        // cioe' quello che l'operatore vede in cima al Full Spectrum.
        var start = Math.max(0, list.length - 30)
        for (var i = list.length - 1; i >= start; --i) {
            var d = list[i]
            if (d.isTx)
                continue
            var call = d.fromCall || ""
            var freq = parseInt(d.freq || "0")
            if (!call || freq <= 0)
                continue
            if (seen[call])
                continue
            seen[call] = true
            // 1.0.138: legge il hex pre-calcolato in enrichDecodeEntry
            // (era: bridge.decodeHighlightBg(d) per ogni call ad ogni
            //  decodeListChanged → 150-600 chiamate/sec, alto CPU/GPU).
            var hex = d.highlightBg || ""
            labels.push({
                call: call,
                freq: freq,
                snr: parseInt(d.db || "0"),
                isCQ: d.isCQ || false,
                isMyCall: d.isMyCall || false,
                color: hex
            })
        }
        waterfallPanel.spectrumDecodeLabels = labels
        waterfallDisplay.setDecodeLabels([])
    }

    Timer {
        id: dxClusterRefreshTimer
        interval: 250
        repeat: false
        onTriggered: {
            if ((bridge.transmitting || bridge.tuning) && waterfallPanel.dxClusterRefreshPending) {
                interval = 1000
                restart()
                return
            }
            waterfallPanel.dxClusterRefreshPending = false
            waterfallPanel.refreshDxClusterSpots()
        }
    }

    function scheduleDxClusterRefresh(delayMs) {
        if (!dxClusterCheck.checked) {
            waterfallPanel.dxClusterRefreshPending = false
            dxClusterRefreshTimer.stop()
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        waterfallPanel.dxClusterRefreshPending = true
        var ms = (delayMs === undefined) ? 250 : delayMs
        dxClusterRefreshTimer.interval = (bridge.transmitting || bridge.tuning) ? 1000 : Math.max(0, ms)
        if (!dxClusterRefreshTimer.running)
            dxClusterRefreshTimer.start()
        else if (!(bridge.transmitting || bridge.tuning))
            dxClusterRefreshTimer.restart()
    }

    // Filtra gli spot del DX cluster per la dial corrente e li passa al
    // PanadapterItem. Ogni voce: { call, freq } con freq in audio Hz.
    // bridge.frequency è la dial in Hz. Spot.frequency è in kHz.
    // Se la dial non è nota (CAT scollegato), inferiamo dalla mediana
    // degli spot intorno a una banda plausibile per non bloccare la feature.
    function refreshDxClusterSpots() {
        if (!waterfallPanel.visible)
            return
        if (!dxClusterCheck.checked) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        if (bridge.transmitting || bridge.tuning) {
            scheduleDxClusterRefresh(1000)
            return
        }
        var spots = bridge.dxClusterSpots || []
        var dialHz = Number(bridge.frequency) || 0
        var fmin = waterfallPanel.minFreq
        var fmax = waterfallPanel.maxFreq
        console.log("[Waterfall][DxCluster] refresh: spots=" + spots.length
                    + " dialHz=" + dialHz + " fmin=" + fmin + " fmax=" + fmax
                    + " toggle=" + dxClusterCheck.checked)
        if (!spots || spots.length === 0) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        // Fallback: se dialHz=0 o molto piccola, prova a usare la freq media
        // degli spot 20m (14000-14400 kHz) come dial pseudo per non perdere
        // visualizzazione totale.
        if (dialHz <= 1000) {
            // Conta spot per banda 20m e usa 14074 come default FT8 dial.
            var has20m = false
            for (var k = 0; k < spots.length; ++k) {
                var fk = Number(spots[k].frequency || 0)
                if (fk >= 14000 && fk <= 14400) { has20m = true; break }
            }
            if (has20m) {
                dialHz = 14074000
                console.log("[Waterfall][DxCluster] fallback dial=14074000Hz (20m FT8)")
            }
        }
        if (dialHz <= 0) {
            waterfallDisplay.setDxClusterSpots([])
            return
        }
        var out = []
        var seen = {}
        var totalRange = 0
        for (var i = spots.length - 1; i >= 0; --i) {
            var s = spots[i]
            if (!s) continue
            var call = (s.dxCall || "").toString().toUpperCase()
            if (!call) continue
            if (seen[call]) continue
            var fkhz = Number(s.frequency || 0)
            if (!fkhz) continue
            var audio = Math.round(fkhz * 1000 - dialHz)
            if (audio < fmin || audio > fmax) continue
            // Skippa spot troppo vicini alla dial (audio<100Hz): non utili
            // come target TX e renderebbero TUNE inaudibile se cliccati.
            if (audio < 100) continue
            seen[call] = true
            out.push({ call: call, freq: audio })
            totalRange++
            if (out.length >= 40) break
        }
        console.log("[Waterfall][DxCluster] inRange=" + totalRange + " sentToWaterfall=" + out.length)
        waterfallDisplay.setDxClusterSpots(out)
    }

    Component.onCompleted: Qt.callLater(loadPanadapterSettings)
    onVisibleChanged: if (visible) Qt.callLater(loadPanadapterSettings)

    // Aggiorna gli spot cluster sul waterfall quando arrivano nuovi spot
    // o quando cambia la dial (cambia anche il filtro audio offset).
    Connections {
        target: bridge
        function onDxClusterSpotsChanged() { waterfallPanel.scheduleDxClusterRefresh(250) }
    }
    Connections {
        target: bridge
        function onFrequencyChanged() {
            if (dxClusterCheck.checked) waterfallPanel.scheduleDxClusterRefresh(120)
        }
        function onTransmittingChanged() {
            if (!bridge.transmitting && waterfallPanel.dxClusterRefreshPending)
                waterfallPanel.scheduleDxClusterRefresh(0)
        }
        function onTuningChanged() {
            if (!bridge.tuning && waterfallPanel.dxClusterRefreshPending)
                waterfallPanel.scheduleDxClusterRefresh(0)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: waterfallPanel.controlsVisible ? 46 : 0
            Layout.bottomMargin: waterfallPanel.controlsVisible ? 6 : 0
            color: wfToolbarBg; visible: waterfallPanel.controlsVisible
            border.color: wfFrame; border.width: 1
            clip: true
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6
                anchors.topMargin: 3; anchors.bottomMargin: 5
                spacing: 6

                Text { text: "Calls:"; color: accentCyan; font.pixelSize: 10; font.bold: true }
                CheckBox {
                    id: showCallsCheck
                    checked: waterfallPanel.showDecodeCallsigns
                    onClicked: waterfallPanel.setShowDecodeCallsigns(checked)
                    ToolTip.text: "Mostra i callsign decodificati sulla cascata"
                    ToolTip.visible: showCallsCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: showCallsCheck.checked ? accentCyan : wfToolbarBg
                        border.color: accentCyan; border.width: 1
                        Text { anchors.centerIn: parent; text: "C"; color: "black"; font.pixelSize: 9; font.bold: true; visible: showCallsCheck.checked }
                    }
                }

                Text { text: "Font"; color: wfText; font.pixelSize: 10 }
                Slider {
                    id: labelFontSlider
                    Layout.preferredWidth: 70
                    Layout.alignment: Qt.AlignVCenter
                    from: 6; to: 20; value: 8; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelFontSize = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelFontSize", value)
                        }
                    }
                    background: Rectangle { x:labelFontSlider.leftPadding; y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-2; width:labelFontSlider.availableWidth; height:4; radius:2; color:wfTrack
                        Rectangle { width: labelFontSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: accentCyan }
                    }
                    handle: Rectangle { x:labelFontSlider.leftPadding+labelFontSlider.visualPosition*(labelFontSlider.availableWidth-width); y:labelFontSlider.topPadding+labelFontSlider.availableHeight/2-height/2; width:10; height:10; radius:5; color: labelFontSlider.pressed ? accentGreen : accentCyan }
                }
                Text { text: labelFontSlider.value.toFixed(0)+"px"; color: accentCyan; font.pixelSize: 10; width: 26 }

                Rectangle { width:1; height:14; color:"#333" }

                Text { text: "Gap"; color: wfText; font.pixelSize: 10 }
                Slider {
                    id: labelSpacingSlider
                    Layout.preferredWidth: 60
                    Layout.alignment: Qt.AlignVCenter
                    from: 0; to: 20; value: 2; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.labelSpacing = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelSpacing", value)
                        }
                    }
                    background: Rectangle { x:labelSpacingSlider.leftPadding; y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-2; width:labelSpacingSlider.availableWidth; height:4; radius:2; color:wfTrack
                        Rectangle { width: labelSpacingSlider.visualPosition*parent.width; height: parent.height; radius: 2; color: accentGreen }
                    }
                    handle: Rectangle { x:labelSpacingSlider.leftPadding+labelSpacingSlider.visualPosition*(labelSpacingSlider.availableWidth-width); y:labelSpacingSlider.topPadding+labelSpacingSlider.availableHeight/2-height/2; width:10; height:10; radius:5; color: labelSpacingSlider.pressed ? accentGreen : accentGreen }
                }
                Text { text: labelSpacingSlider.value.toFixed(0); color: accentGreen; font.pixelSize: 10; width: 20 }

                Rectangle { width:1; height:14; color:"#333" }

                CheckBox {
                    id: labelBoldCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.labelBold = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelBold", checked)
                        }
                    }
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: labelBoldCheck.checked ? "#FFFFFF" : wfToolbarBg
                        border.color: "#FFFFFF"; border.width: 1
                        Text { anchors.centerIn: parent; text: "B"; color: "black"; font.pixelSize: 9; font.bold: true; visible: labelBoldCheck.checked }
                    }
                }
                Text { text: "Bold"; color: labelBoldCheck.checked ? "#FFFFFF" : textSec; font.pixelSize: 10 }

                Rectangle { width:1; height:14; color:"#333" }

                Text { text: "Color"; color: wfText; font.pixelSize: 10 }
                ComboBox {
                    id: labelColorCombo
                    Layout.preferredWidth: 86
                    Layout.alignment: Qt.AlignVCenter
                    font.pixelSize: 10
                    model: waterfallPanel.labelColorPresets.map(function(p){ return p.name })
                    currentIndex: 0
                    onActivated: {
                        var preset = waterfallPanel.labelColorPresets[currentIndex]
                        waterfallDisplay.labelUseCustomColor = preset.custom
                        waterfallDisplay.labelColor = preset.color
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiLabelColorPreset", currentIndex)
                        }
                    }
                    background: Rectangle { color: wfToolbarBg; border.color: borderColor; radius: 2 }
                    contentItem: Row {
                        spacing: 4
                        leftPadding: 4
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 10; height: 10; radius: 2
                            color: waterfallPanel.labelColorPresets[labelColorCombo.currentIndex].color
                            border.color: "#555"; border.width: 1
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: labelColorCombo.displayText
                            font.pixelSize: 10; color: textPrimary
                        }
                    }
                }

                Rectangle { width:1; height:14; color:"#333" }

                // ── Palette / Auto / [] / Peak / Zoom / dBm ──
                // Unita alla barra Calls per avere un'unica riga superiore (feedback IK8OLM)
                Text { text: "Palette:"; color: wfText; font.pixelSize: 10 }
                ComboBox {
                    id: paletteCombo
                    Layout.preferredWidth: 106
                    model: waterfallDisplay.paletteNames
                    currentIndex: 0
                    font.pixelSize: 10
                    onActivated: {
                        waterfallPanel.setPaletteIndex(currentIndex, true)
                    }
                    background: Rectangle { color: wfToolbarBg; border.color: borderColor; radius: 2 }
                    contentItem: Text { text: paletteCombo.displayText; font.pixelSize: 10; color: textPrimary; verticalAlignment: Text.AlignVCenter; leftPadding: 4 }
                }

                // Auto Range
                CheckBox {
                    id: autoRangeCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.autoRange = checked
                        if (!checked) {
                            waterfallPanel.applyManualContrast()
                        }
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallAutoRange", checked)
                        }
                    }
                    ToolTip.text: "Soglia di rumore automatica (IIR)"
                    ToolTip.visible: autoRangeCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: autoRangeCheck.checked ? accentGreen : wfToolbarBg
                        border.color: accentGreen; border.width: 1
                        Text { anchors.centerIn: parent; text: "A"; color: "black"; font.pixelSize: 9; font.bold: true; visible: autoRangeCheck.checked }
                    }
                }
                Text { text: "Auto"; color: autoRangeCheck.checked ? accentGreen : textSec; font.pixelSize: 10 }

                // TX brackets toggle
                Text { text: "[ ]"; color: txBracketsCheck.checked ? accentGreen : textSec; font.pixelSize: 10; font.bold: true }
                CheckBox {
                    id: txBracketsCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.showTxBrackets = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallShowTxBrackets", checked)
                        }
                    }
                    indicator: Rectangle {
                        implicitWidth: 16; implicitHeight: 16; radius: 2
                        color: txBracketsCheck.checked ? accentGreen : wfToolbarBg
                        border.color: accentGreen; border.width: 1
                        Text { anchors.centerIn: parent; text: "✓"; color: "white"; font.pixelSize: 10; visible: txBracketsCheck.checked }
                    }
                }

                // DX Cluster spots toggle (giallo, click = chiama stazione)
                CheckBox {
                    id: dxClusterCheck
                    checked: false
                    onCheckedChanged: {
                        waterfallDisplay.showDxClusterSpots = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallShowDxCluster", checked)
                        }
                        if (checked) {
                            waterfallPanel.scheduleDxClusterRefresh(0)
                        }
                    }
                    ToolTip.text: "Mostra gli spot del DX Cluster sulla cascata (clicca per chiamare)"
                    ToolTip.visible: dxClusterCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: dxClusterCheck.checked ? "#FFC800" : wfToolbarBg
                        border.color: "#FFC800"; border.width: 1
                        Text { anchors.centerIn: parent; text: "DX"; color: "black"; font.pixelSize: 7; font.bold: true; visible: dxClusterCheck.checked }
                    }
                }
                Text { text: "Cluster"; color: dxClusterCheck.checked ? "#FFC800" : textSec; font.pixelSize: 10 }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: collapseControlsButton
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    radius: 6
                    color: collapseControlsMA.containsMouse
                           ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.26)
                           : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.86)
                    border.color: accentGreen
                    border.width: collapseControlsMA.containsMouse ? 2 : 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: qsTr("Hide")
                            color: accentGreen
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "\u25B4"
                            color: accentGreen
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: collapseControlsMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: waterfallPanel.setControlsExpanded(false)
                    }

                    ToolTip.visible: collapseControlsMA.containsMouse
                    ToolTip.delay: 450
                    ToolTip.text: qsTr("Nascondi i controlli della cascata")
                }

                // Peak Hold toggle
                CheckBox {
                    id: peakHoldCheck
                    checked: true
                    onCheckedChanged: {
                        waterfallDisplay.peakHold = checked
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallPeakHold", checked)
                        }
                    }
                    ToolTip.text: "Peak Hold: mantiene visibili i picchi dello spettro"
                    ToolTip.visible: peakHoldCheck.hovered
                    ToolTip.delay: 400
                    indicator: Rectangle {
                        implicitWidth: 14; implicitHeight: 14; radius: 2
                        color: peakHoldCheck.checked ? wfYellow : wfToolbarBg
                        border.color: wfYellow; border.width: 1
                        Text { anchors.centerIn: parent; text: "P"; color: "#262626"; font.pixelSize: 9; font.bold: true; visible: peakHoldCheck.checked }
                    }
                }
                Text { text: "Peak"; color: peakHoldCheck.checked ? wfYellow : textSec; font.pixelSize: 10 }

                // Zoom
                Text { text: "Zoom"; color: wfText; font.pixelSize: 10 }
                Slider {
                    id: zoomSlider
                    Layout.preferredWidth: 70
                    from: 1; to: 8; value: bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0; stepSize: 0.5
                    onValueChanged: {
                        waterfallDisplay.zoomFactor = value
                        if (!waterfallPanel.restoringSettings) {
                            bridge.uiZoomFactor = value
                            waterfallPanel.persistGraphSetting("uiZoomFactor", value)
                        }
                    }
                    background: Rectangle {
                        x: zoomSlider.leftPadding; y: zoomSlider.topPadding + zoomSlider.availableHeight/2 - 2
                        width: zoomSlider.availableWidth; height: 4; radius: 2
                        color: Qt.rgba(textPrimary.r,textPrimary.g,textPrimary.b,0.15)
                        Rectangle { width: zoomSlider.visualPosition * parent.width; height: parent.height; radius: 2; color: accentCyan }
                    }
                    handle: Rectangle {
                        x: zoomSlider.leftPadding + zoomSlider.visualPosition*(zoomSlider.availableWidth - width)
                        y: zoomSlider.topPadding + zoomSlider.availableHeight/2 - height/2
                        width: 10; height: 10; radius: 5
                        color: zoomSlider.pressed ? accentGreen : accentCyan
                    }
                }
                Text { text: zoomSlider.value.toFixed(1)+"×"; color: accentCyan; font.pixelSize: 10; width: 28 }

                // dBm live noise floor
                Text {
                    text: waterfallDisplay.measuredFloor.toFixed(0) + "dBm"
                    color: accentGreen
                    font.pixelSize: 10
                    ToolTip.text: "Soglia di rumore misurata"
                    ToolTip.visible: nfLabel.containsMouse
                    ToolTip.delay: 400
                    MouseArea { id: nfLabel; anchors.fill: parent; hoverEnabled: true }
                }
            }
        }

        // ── Slider Black / Gain ─────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: waterfallPanel.controlsVisible ? 22 : 0
            color: wfToolbarBg; visible: waterfallPanel.controlsVisible
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 6
                Text { text: "Black:"; color: wfSlate; font.pixelSize: 10 }
                Slider { id: blackSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 15; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.blackLevel = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallBlackLevel", value)
                        }
                    }
                    background: Rectangle { x:blackSlider.leftPadding;y:blackSlider.topPadding+blackSlider.availableHeight/2-2;width:blackSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:blackSlider.leftPadding+blackSlider.visualPosition*(blackSlider.availableWidth-width);y:blackSlider.topPadding+blackSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfSlate }
                }
                Text { text: blackSlider.value.toFixed(0); color: wfSlate; font.pixelSize: 10; width: 20 }
                Rectangle { width:1;height:14;color:"#333" }
                Text { text: "Gain:"; color: wfBlue; font.pixelSize: 10 }
                Slider { id: gainSlider; Layout.preferredWidth: 80; from: 0; to: 100; value: 50; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.colorGain = value
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallColorGain", value)
                        }
                    }
                    background: Rectangle { x:gainSlider.leftPadding;y:gainSlider.topPadding+gainSlider.availableHeight/2-2;width:gainSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:gainSlider.leftPadding+gainSlider.visualPosition*(gainSlider.availableWidth-width);y:gainSlider.topPadding+gainSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfBlue }
                }
                Text { text: gainSlider.value.toFixed(0); color: wfBlue; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: "Contrasto:"; color: wfPurple; font.pixelSize: 10 }
                Slider { id: contrastSlider; Layout.preferredWidth: 70; from: 10; to: 150; value: 80; stepSize: 1
                    onValueChanged: {
                        waterfallDisplay.contrastLevel = value
                        waterfallPanel.applyManualContrast()
                        if (!waterfallPanel.restoringSettings) {
                            waterfallPanel.persistGraphSetting("uiWaterfallContrast", value)
                        }
                    }
                    background: Rectangle { x:contrastSlider.leftPadding;y:contrastSlider.topPadding+contrastSlider.availableHeight/2-2;width:contrastSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:contrastSlider.leftPadding+contrastSlider.visualPosition*(contrastSlider.availableWidth-width);y:contrastSlider.topPadding+contrastSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:wfPurple }
                }
                Text { text: contrastSlider.value.toFixed(0); color: wfPurple; font.pixelSize: 10; width: 20 }

                Rectangle { width:1;height:14;color:"#333" }

                Text { text: "Vel:"; color: "#DD8866"; font.pixelSize: 10 }
                Slider { id: speedSlider; Layout.preferredWidth: 60; from: 10; to: 500; value: 20; stepSize: 5
                    onValueChanged: if (!waterfallPanel.restoringSettings) waterfallPanel.persistGraphSetting("spectrumInterval", value)
                    background: Rectangle { x:speedSlider.leftPadding;y:speedSlider.topPadding+speedSlider.availableHeight/2-2;width:speedSlider.availableWidth;height:4;radius:2;color:wfTrack }
                    handle: Rectangle { x:speedSlider.leftPadding+speedSlider.visualPosition*(speedSlider.availableWidth-width);y:speedSlider.topPadding+speedSlider.availableHeight/2-height/2;width:10;height:10;radius:5;color:"#DD8866" }
                }
                Text { text: speedSlider.value.toFixed(0)+"ms"; color: "#DD8866"; font.pixelSize: 10; width: 32 }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Divisore draggabile tra spectrum e waterfall ──────────────────
        // Trascina per regolare l'altezza del grafico spettro
        Rectangle {
            id: spectrumDivider
            Layout.fillWidth: true
            Layout.preferredHeight: waterfallPanel.controlsVisible ? 12 : 0
            visible: waterfallPanel.controlsVisible
            color: dividerMouse.containsMouse || dividerMouse.pressed
                   ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.30) : Qt.rgba(1,1,1,0.06)

            // 1.0.288 — grip più evidente: trascina questa barra per regolare quanto
            // spazio dare allo spettro (in alto) vs la cascata (in basso).
            Rectangle {
                anchors.centerIn: parent
                width: 100; height: 4; radius: 2
                color: accentCyan
                opacity: dividerMouse.containsMouse || dividerMouse.pressed ? 1.0 : 0.75
            }

            MouseArea {
                id: dividerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor
                property int dragStartY: 0
                property int dragStartH: 0

                onPressed: function(mouse) {
                    dragStartY = mouse.y + spectrumDivider.mapToItem(waterfallPanel, 0, 0).y
                    dragStartH = waterfallPanel.spectrumHeight
                }
                // 1.0.288 — BUG FIX: era onMouseYChanged con `mouse.y`, ma in quell'handler
                // il parametro `mouse` non esiste (è property-change) → TypeError → il drag
                // non aggiornava mai spectrumHeight (split mai funzionante). Uso onPositionChanged
                // (che riceve `mouse`) e mouseY come fallback.
                onPositionChanged: function(mouse) {
                    if (pressed) {
                        var dy = (mouse.y + spectrumDivider.mapToItem(waterfallPanel, 0, 0).y) - dragStartY
                        var newH = dragStartH + dy
                        var newVal = Math.round(Math.max(waterfallPanel.spectrumMinHeight,
                                             Math.min(waterfallPanel.spectrumMaxHeight, newH)))
                        waterfallPanel.spectrumHeight = newVal
                        bridge.uiSpectrumHeight = newVal
                        mainWindow.scheduleSave()
                    }
                }
                onReleased: {
                    waterfallPanel.persistGraphSetting("uiSpectrumHeight", waterfallPanel.spectrumHeight)
                }
            }

            Behavior on color { ColorAnimation { duration: bridge.lowCpuModeEnabled ? 0 : 100 } }
        }

        // ── PanadapterItem — FlexRadio SmartSDR style ─────────────────────
        PanadapterItem {
            id: waterfallDisplay
            Layout.fillWidth: true
            Layout.fillHeight: true

            startFreq:      waterfallPanel.minFreq
            bandwidth:      waterfallPanel.maxFreq - waterfallPanel.minFreq
            rxFreq:         bridge.rxFrequency
            txFreq:         bridge.txFrequency
            running:        bridge.monitoring
            showTxBrackets: true
            spectrumHeight: Math.max(waterfallPanel.spectrumMinHeight,
                                     Math.min(waterfallPanel.spectrumHeight,
                                              Math.max(waterfallPanel.spectrumMinHeight,
                                                       waterfallDisplay.height - waterfallPanel.waterfallMinHeight)))
            // Low CPU mode riattiva un throttle leggero per contenere il render
            // QML/RHI sui PC datati. A profilo normale resta fluido a pieno rate.
            throttleActive: bridge.lowCpuModeEnabled
            throttleIntervalMs: bridge.lowCpuModeEnabled ? 250 : 100
            // Carica valori da Settings al primo avvio.
            paletteIndex:   0
            contrastLevel:  contrastSlider.value

            // In light theme forza la palette pastello chiara (indice 11) per coerenza visiva col mockup.
            // Binding esplicito: ha priorità sopra qualsiasi assegnazione procedurale finché when=true.
            Binding {
                target: waterfallDisplay
                property: "paletteIndex"
                value: 11
                when: bridge.themeManager.isLightTheme
                restoreMode: Binding.RestoreBindingOrValue
            }
            Component.onCompleted: {
                console.log("[Waterfall] theme=" + bridge.themeManager.currentTheme
                    + " isLight=" + bridge.themeManager.isLightTheme
                    + " paletteIndex=" + waterfallDisplay.paletteIndex)
            }
            Connections {
                target: bridge.themeManager
                function onPaletteChanged() {
                    console.log("[Waterfall] paletteChanged → theme=" + bridge.themeManager.currentTheme
                        + " isLight=" + bridge.themeManager.isLightTheme
                        + " paletteIndex=" + waterfallDisplay.paletteIndex)
                }
            }
            autoRange:      autoRangeCheck.checked
            peakHold:       true
            zoomFactor:     bridge.uiZoomFactor > 0 ? bridge.uiZoomFactor : 1.0

            readonly property real rxFilterHz: 300
            readonly property real txSignalBandwidthHz: waterfallPanel.txSignalBandwidthHz(bridge.mode)
            readonly property real viewRangeHz: Math.max(1, bandwidth) / Math.max(0.001, zoomFactor)
            readonly property real viewStartHz: startFreq + Math.max(1, bandwidth) * 0.5 + panHz - viewRangeHz * 0.5
            readonly property real rxFilterLeftX: Math.max(0, Math.min(width, freqToPixel(rxFreq - rxFilterHz * 0.5)))
            readonly property real rxFilterRightX: Math.max(0, Math.min(width, freqToPixel(rxFreq + rxFilterHz * 0.5)))
            readonly property real txSignalLeftX: freqToPixel(txFreq - txSignalBandwidthHz * 0.5)
            readonly property real txSignalRightX: freqToPixel(txFreq + txSignalBandwidthHz * 0.5)
            readonly property bool txSignalGuideVisible: showTxBrackets
                                                           && txSignalBandwidthHz > 0
                                                           && txSignalRightX >= 0
                                                           && txSignalLeftX <= width

            function freqToPixel(freq) {
                return (Number(freq) - viewStartHz) * width / viewRangeHz
            }

            // Salva al bridge con debounce (2s dopo l'ultimo cambio)
            // Non persistere la palette light auto-attivata col tema (resta una scelta del tema, non utente).
            onPaletteIndexChanged: {
                if (!waterfallPanel.restoringSettings
                    && !waterfallPanel.syncingPaletteChoice
                    && !bridge.themeManager.isLightTheme) {
                    waterfallPanel.setPaletteIndex(paletteIndex, true)
                }
            }
            onZoomFactorChanged: if (!waterfallPanel.restoringSettings) {
                bridge.uiZoomFactor = zoomFactor
                waterfallPanel.persistGraphSetting("uiZoomFactor", zoomFactor)
            }
            onMeasuredFloorChanged: waterfallPanel.applyManualContrast()
            onGpuFftUnavailable: function(reason) {
                bridge.setGpuPanadapterFftAvailable(false, reason)
            }

            onFrequencySelected: function(freq) {
                waterfallPanel.frequencySelected(freq)        // RX
            }
            onTxFrequencySelected: function(freq) {
                waterfallPanel.txFrequencySelected(freq)      // TX
            }
            onDxClusterSpotClicked: function(call, audioFreqHz) {
                console.log("[Waterfall] DX cluster click → engage", call, "@", audioFreqHz, "Hz")
                bridge.engageDxClusterSpot(call, audioFreqHz)
            }
            onDecodeLabelClicked: function(call, audioFreqHz) {
                console.log("[Waterfall] decode label click → engage", call, "@", audioFreqHz, "Hz")
                bridge.engageDxClusterSpot(call, audioFreqHz)
            }

            Item {
                id: spectrumGpuOverlay
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: Math.max(0, Math.min(waterfallDisplay.spectrumHeight, waterfallDisplay.height))
                z: 10
                clip: true
                visible: waterfallDisplay.spectrumGpuOverlayAvailable && height > 0
                readonly property bool cppOverlayEnabled: waterfallDisplay.spectrumGpuOverlayAvailable
                readonly property bool nativeDecodeLabelsEnabled: true

                readonly property string fixedFontFamily: decodiumMonoFontFamily
                readonly property real viewStartHz: waterfallDisplay.viewStartHz
                readonly property real viewRangeHz: waterfallDisplay.viewRangeHz
                readonly property real dbRange: Math.max(1, waterfallDisplay.maxDb - waterfallDisplay.minDb)
                readonly property int freqStep: viewRangeHz > 3000 ? 500 : (viewRangeHz > 1000 ? 200 : 100)
                readonly property bool txVisible: {
                    var txX = xForFreq(waterfallDisplay.txFreq)
                    return txX >= 0 && txX < width && waterfallDisplay.txFreq !== waterfallDisplay.rxFreq
                }

                function xForFreq(freq) {
                    return (Number(freq) - viewStartHz) * width / viewRangeHz
                }

                function clamp(value, minValue, maxValue) {
                    return Math.max(minValue, Math.min(maxValue, value))
                }

                function markerBoxX(markerX, boxWidth) {
                    var boxX = markerX + 8
                    if (boxX + boxWidth > width - 2)
                        boxX = markerX - boxWidth - 8
                    return clamp(boxX, 2, Math.max(2, width - boxWidth - 2))
                }

                function markerBoxY(centerY, boxHeight) {
                    var minY = boxHeight / 2 + 3
                    var maxY = Math.max(minY, height - boxHeight / 2 - 22)
                    return clamp(centerY, minY, maxY) - boxHeight / 2
                }

                function frequencyGridModel() {
                    var out = []
                    var first = (Math.floor(viewStartHz / freqStep) + 1) * freqStep
                    var end = viewStartHz + viewRangeHz
                    for (var f = first; f < end; f += freqStep) {
                        var x = xForFreq(f)
                        if (x < 0 || x >= width)
                            continue
                        out.push({
                            x: Math.round(x),
                            label: f >= 1000 ? (f / 1000).toFixed(1) + "k" : String(Math.round(f))
                        })
                    }
                    return out
                }

                function tickModel() {
                    var out = []
                    var first = Math.floor(viewStartHz / 500) * 500 + 500
                    var end = viewStartHz + viewRangeHz
                    for (var f = first; f < end; f += 500) {
                        var x = xForFreq(f)
                        if (x < 0 || x >= width)
                            continue
                        out.push({ x: Math.round(x), label: String(Math.round(f)) })
                    }
                    return out
                }

                function decodeColor(label) {
                    if (waterfallDisplay.labelUseCustomColor)
                        return waterfallDisplay.labelColor
                    if (label.isMyCall)
                        return "#ff5050"
                    if (label.isCQ)
                        return "#00e664"
                    return "#00c8ff"
                }

                property var cachedFrequencyGridModel: []
                property var cachedTickModel: []

                Timer {
                    id: staticOverlayRefreshTimer
                    interval: 120
                    repeat: false
                    onTriggered: {
                        if (spectrumGpuOverlay.cppOverlayEnabled)
                            return
                        spectrumGpuOverlay.cachedFrequencyGridModel =
                            spectrumGpuOverlay.frequencyGridModel()
                        spectrumGpuOverlay.cachedTickModel =
                            spectrumGpuOverlay.tickModel()
                    }
                }

                function scheduleStaticOverlayRefresh() {
                    if (cppOverlayEnabled)
                        return
                    if (!staticOverlayRefreshTimer.running)
                        staticOverlayRefreshTimer.start()
                }

                // 1.0.175 — Cache del label model con throttle 4 Hz.
                // Su banda affollata (FT2 async 5-10 decode/sec) il sort+pack
                // di decodeLabelModel() veniva chiamato troppo spesso, in
                // competizione col paint del waterfall → label che saltano
                // frame e si "accavallano". Cachiamo il risultato e
                // refreshiamo al massimo 4 volte al secondo.
                property var cachedDecodeLabelModel: []

                Timer {
                    id: decodeLabelRefreshTimer
                    interval: 250
                    repeat: false
                    onTriggered: {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        spectrumGpuOverlay.cachedDecodeLabelModel =
                            spectrumGpuOverlay.decodeLabelModel()
                    }
                }

                Connections {
                    target: waterfallPanel
                    function onSpectrumDecodeLabelsChanged() {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        if (!waterfallPanel.showDecodeCallsigns) {
                            decodeLabelRefreshTimer.stop()
                            spectrumGpuOverlay.cachedDecodeLabelModel = []
                            return
                        }
                        if (!decodeLabelRefreshTimer.running)
                            decodeLabelRefreshTimer.start()
                    }
                    function onShowDecodeCallsignsChanged() {
                        if (spectrumGpuOverlay.cppOverlayEnabled && !spectrumGpuOverlay.nativeDecodeLabelsEnabled)
                            return
                        if (!waterfallPanel.showDecodeCallsigns) {
                            decodeLabelRefreshTimer.stop()
                            spectrumGpuOverlay.cachedDecodeLabelModel = []
                            return
                        }
                        if (!decodeLabelRefreshTimer.running)
                            decodeLabelRefreshTimer.start()
                    }
                }

                onWidthChanged:  {
                    if (!cppOverlayEnabled || nativeDecodeLabelsEnabled) {
                        if (!decodeLabelRefreshTimer.running) decodeLabelRefreshTimer.start()
                    }
                    if (!cppOverlayEnabled) {
                        scheduleStaticOverlayRefresh()
                    }
                }
                onHeightChanged: {
                    if (!cppOverlayEnabled || nativeDecodeLabelsEnabled) {
                        if (!decodeLabelRefreshTimer.running) decodeLabelRefreshTimer.start()
                    }
                    if (!cppOverlayEnabled) {
                        scheduleStaticOverlayRefresh()
                    }
                }
                onViewStartHzChanged: scheduleStaticOverlayRefresh()
                onViewRangeHzChanged: scheduleStaticOverlayRefresh()

                Component.onCompleted: {
                    if (cppOverlayEnabled) {
                        cachedFrequencyGridModel = []
                        cachedTickModel = []
                        cachedDecodeLabelModel = nativeDecodeLabelsEnabled ? decodeLabelModel() : []
                    } else {
                        cachedFrequencyGridModel = frequencyGridModel()
                        cachedTickModel = tickModel()
                        cachedDecodeLabelModel = decodeLabelModel()
                    }
                }

                function decodeLabelModel() {
                    if (!waterfallPanel.showDecodeCallsigns)
                        return []

                    var source = waterfallPanel.spectrumDecodeLabels || []
                    var items = []
                    var i
                    for (i = 0; i < source.length; ++i) {
                        var d = source[i]
                        var call = d.call || ""
                        var freq = Number(d.freq || 0)
                        if (!call || freq <= 0)
                            continue
                        var x = xForFreq(freq)
                        if (x < 0 || x >= width)
                            continue
                        var text = call + " " + Number(d.snr || 0)
                        var effectiveLabelFontSize = Math.max(10, waterfallDisplay.labelFontSize)
                        items.push({
                            x: Math.round(x),
                            text: text,
                            color: decodeColor(d),
                            call: call,
                            freq: freq,
                            widthHint: Math.max(24, text.length * Math.max(6, effectiveLabelFontSize * 0.68))
                        })
                    }

                    items.sort(function(a, b) { return a.x - b.x })

                    var rowHeight = Math.max(12, Math.max(10, waterfallDisplay.labelFontSize) + 5)
                    var maxRows = Math.max(1, Math.floor(Math.max(1, height - 24) / rowHeight))
                    var rowRight = []
                    for (i = 0; i < maxRows; ++i)
                        rowRight.push(-1000000)

                    var laidOut = []
                    for (i = 0; i < items.length; ++i) {
                        var it = items[i]
                        var textX = clamp(it.x + 2, 2, Math.max(2, width - it.widthHint - 2))
                        var row = -1
                        for (var r = 0; r < maxRows; ++r) {
                            if (textX > rowRight[r] + waterfallDisplay.labelSpacing) {
                                row = r
                                break
                            }
                        }
                        if (row < 0)
                            continue
                        laidOut.push({
                            x: it.x,
                            textX: textX,
                            y: 2 + row * rowHeight,
                            text: it.text,
                            color: it.color,
                            call: it.call,
                            freq: it.freq,
                            widthHint: it.widthHint,
                            rowHeight: rowHeight
                        })
                        rowRight[row] = textX + it.widthHint
                    }
                    return laidOut
                }

                Rectangle {
                    x: waterfallDisplay.rxFilterLeftX
                    y: 0
                    width: Math.max(0, waterfallDisplay.rxFilterRightX - waterfallDisplay.rxFilterLeftX)
                    height: parent.height
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && width > 0
                    color: Qt.rgba(0.31, 0.43, 0.47, 0.09)
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? 0 : 6
                    Rectangle {
                        width: spectrumGpuOverlay.width
                        height: 1
                        x: 0
                        y: Math.round(spectrumGpuOverlay.height - 1 - (index / 5) * (spectrumGpuOverlay.height - 16))
                        color: "#262626"
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? 0 : 6
                    Text {
                        readonly property real norm: index / 5
                        x: 2
                        y: Math.max(0, Math.round(spectrumGpuOverlay.height - 1 - norm * (spectrumGpuOverlay.height - 16)) - height)
                        text: String(Math.round(waterfallDisplay.minDb + norm * spectrumGpuOverlay.dbRange))
                        color: "#a0a0a0"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 8
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedFrequencyGridModel
                    Rectangle {
                        x: modelData.x
                        y: 0
                        width: 1
                        height: Math.max(0, spectrumGpuOverlay.height - 16)
                        color: "#282828"
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedFrequencyGridModel
                    Text {
                        x: spectrumGpuOverlay.clamp(modelData.x - 18, 2, Math.max(2, spectrumGpuOverlay.width - width - 2))
                        y: Math.max(0, spectrumGpuOverlay.height - height - 1)
                        text: modelData.label
                        color: "#dcdcdc"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedTickModel
                    Rectangle {
                        x: modelData.x
                        y: Math.max(0, spectrumGpuOverlay.height - 18)
                        width: 1
                        height: 12
                        color: "#ffe600"
                        opacity: 0.70
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedTickModel
                    Text {
                        x: spectrumGpuOverlay.clamp(modelData.x - 20, 2, Math.max(2, spectrumGpuOverlay.width - width - 2))
                        y: Math.max(0, spectrumGpuOverlay.height - 36)
                        text: modelData.label
                        color: "#dcd200"
                        font.family: spectrumGpuOverlay.fixedFontFamily
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Repeater {
                    model: spectrumGpuOverlay.nativeDecodeLabelsEnabled
                           ? spectrumGpuOverlay.cachedDecodeLabelModel
                           : (spectrumGpuOverlay.cppOverlayEnabled ? [] : spectrumGpuOverlay.cachedDecodeLabelModel)
                    Item {
                        width: spectrumGpuOverlay.width
                        height: spectrumGpuOverlay.height
                        Rectangle {
                            x: modelData.x
                            y: 0
                            width: 1
                            height: Math.max(0, spectrumGpuOverlay.height - 20)
                            color: modelData.color
                            opacity: 0.52
                        }
                        Rectangle {
                            x: Math.round(modelData.textX - 2)
                            y: Math.round(modelData.y - 1)
                            width: Math.round(modelData.widthHint + 4)
                            height: Math.round(modelData.rowHeight)
                            color: "#000000"
                            opacity: 0.48
                            radius: 0
                        }
                        Text {
                            x: Math.round(modelData.textX)
                            y: Math.round(modelData.y)
                            text: modelData.text
                            color: modelData.color
                            font.family: spectrumGpuOverlay.fixedFontFamily
                            font.pixelSize: Math.max(10, waterfallDisplay.labelFontSize)
                            font.bold: waterfallDisplay.labelBold
                            font.hintingPreference: Font.PreferFullHinting
                            renderType: Text.NativeRendering
                        }
                        MouseArea {
                            x: Math.round(modelData.textX - 3)
                            y: Math.round(modelData.y - 2)
                            width: Math.round(modelData.widthHint + 6)
                            height: Math.round(modelData.rowHeight + 2)
                            acceptedButtons: Qt.LeftButton
                            propagateComposedEvents: true
                            cursorShape: Qt.PointingHandCursor
                            // 1.0.337: il left-click SEMPLICE passa all'item C++ (imposta TX);
                            // solo Ctrl+click chiama la stazione (engage).
                            onPressed: function(mouse) { mouse.accepted = !!(mouse.modifiers & Qt.ControlModifier) }
                            onClicked: function(mouse) {
                                if (mouse.modifiers & Qt.ControlModifier)
                                    bridge.engageDxClusterSpot(modelData.call, modelData.freq)
                            }
                        }
                    }
                }

                Item {
                    id: rxMarkerLine
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.rxFreq)
                    x: Math.round(markerX)
                    y: 0
                    width: 1
                    height: parent.height
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    Rectangle { x: -3; width: 7; height: parent.height; color: "#00e5ff"; opacity: 0.27 }
                    Rectangle { x: -1; width: 3; height: parent.height; color: "#00e5ff"; opacity: 0.94 }
                    Rectangle { x: 0; width: 1; height: parent.height; color: "#b4ffff" }
                }

                Item {
                    id: txMarkerLine
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    x: Math.round(markerX)
                    y: 0
                    width: 1
                    height: parent.height
                    // 1.0.288 — marker TX magenta nascosto: l'UNICO indicatore TX è la
                    // linea rossa animata (txCarrierActiveBar), visibile solo in trasmissione.
                    visible: false
                    Rectangle { x: -3; width: 7; height: parent.height; color: "#ff00ff"; opacity: 0.27 }
                    Rectangle { x: -1; width: 3; height: parent.height; color: "#ff00ff"; opacity: 0.94 }
                    Rectangle { x: 0; width: 1; height: parent.height; color: "#ffc8ff" }
                }

                Item {
                    id: txSignalWidthGuide
                    z: -1
                    anchors.fill: parent
                    // 1.0.288 — guida banda TX magenta nascosta: l'unico indicatore TX è
                    // la linea rossa animata (txCarrierActiveBar), visibile solo in TX.
                    visible: false

                    readonly property real leftX: waterfallDisplay.txSignalLeftX
                    readonly property real rightX: waterfallDisplay.txSignalRightX
                    readonly property real clampedLeftX: spectrumGpuOverlay.clamp(leftX, 0, spectrumGpuOverlay.width)
                    readonly property real clampedRightX: spectrumGpuOverlay.clamp(rightX, 0, spectrumGpuOverlay.width)

                    Rectangle {
                        x: txSignalWidthGuide.clampedLeftX
                        y: 0
                        width: Math.max(1, txSignalWidthGuide.clampedRightX - txSignalWidthGuide.clampedLeftX)
                        height: parent.height
                        color: Qt.rgba(1.0, 0.0, 1.0, 0.055)
                    }
                    Rectangle {
                        x: Math.round(txSignalWidthGuide.leftX) - 1
                        y: 0
                        width: 2
                        height: parent.height
                        visible: txSignalWidthGuide.leftX >= 0 && txSignalWidthGuide.leftX <= spectrumGpuOverlay.width
                        color: Qt.rgba(1.0, 0.40, 1.0, 0.82)
                    }
                    Rectangle {
                        x: Math.round(txSignalWidthGuide.rightX) - 1
                        y: 0
                        width: 2
                        height: parent.height
                        visible: txSignalWidthGuide.rightX >= 0 && txSignalWidthGuide.rightX <= spectrumGpuOverlay.width
                        color: Qt.rgba(1.0, 0.40, 1.0, 0.82)
                    }
                }

                // 1.0.269 (fork-only) — TX CARRIER ACTIVE: barra rossa larga ~50Hz
                // (banda della portante FT8) che si accende SOLO quando bridge.transmitting=true
                // o bridge.tuning=true. Indica visivamente che la portante e' on-air.
                // 1.0.270: z=50 per disegnare SOPRA rxMarkerLabel/txMarkerLabel (z=0)
                // che venivano dichiarati dopo e coprivano la banda rossa.
                Item {
                    id: txCarrierActiveBar
                    z: 50
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    readonly property real carrierBwHz: Math.max(50, waterfallDisplay.txSignalBandwidthHz)
                    readonly property real barWidthPx: Math.max(8, carrierBwHz * spectrumGpuOverlay.width / Math.max(1, spectrumGpuOverlay.viewRangeHz))
                    x: Math.round(markerX - barWidthPx / 2)
                    y: 0
                    width: barWidthPx
                    height: parent.height
                    visible: bridge && (bridge.transmitting || bridge.tuning)
                             && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    // 1.0.288 — pulsazione animata: "linea rossa animata" che indica la TX in corso.
                    SequentialAnimation on opacity {
                        running: txCarrierActiveBar.visible
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.45; duration: 400; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.45; to: 1.0; duration: 400; easing.type: Easing.InOutSine }
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: Qt.rgba(1.0, 0.12, 0.12, 0.32)
                        border.color: Qt.rgba(1.0, 0.20, 0.20, 0.95)
                        border.width: 2
                    }
                    Rectangle {
                        x: parent.width / 2 - 1
                        width: 2
                        height: parent.height
                        color: Qt.rgba(1.0, 0.25, 0.25, 0.98)
                    }
                    // Etichetta "TX ON-AIR" sopra
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 2
                        width: txOnAirText.implicitWidth + 10
                        height: txOnAirText.implicitHeight + 4
                        radius: 3
                        color: Qt.rgba(0.85, 0.05, 0.05, 0.95)
                        Text {
                            id: txOnAirText
                            anchors.centerIn: parent
                            text: bridge && bridge.tuning ? "TUNE" : "TX ON-AIR"
                            color: "#ffffff"
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }
                }

                Item {
                    id: rxMarkerLabel
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.rxFreq)
                    readonly property real centerY: spectrumGpuOverlay.height / 2 - (spectrumGpuOverlay.txVisible ? 12 : 0)
                    width: rxMarkerText.implicitWidth + 12
                    height: rxMarkerText.implicitHeight + 6
                    x: spectrumGpuOverlay.markerBoxX(markerX, width)
                    y: spectrumGpuOverlay.markerBoxY(centerY, height)
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && markerX >= 0 && markerX < spectrumGpuOverlay.width
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Qt.rgba(0, 0, 0, 0.70)
                        border.color: "#00e5ff"
                        border.width: 1
                    }
                    Text {
                        id: rxMarkerText
                        anchors.centerIn: parent
                        text: "RX " + Math.round(waterfallDisplay.rxFreq)
                        color: "#00e5ff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Item {
                    id: txMarkerLabel
                    readonly property real markerX: spectrumGpuOverlay.xForFreq(waterfallDisplay.txFreq)
                    readonly property real centerY: spectrumGpuOverlay.height / 2 + 12
                    width: txMarkerText.implicitWidth + 12
                    height: txMarkerText.implicitHeight + 6
                    x: spectrumGpuOverlay.markerBoxX(markerX, width)
                    y: spectrumGpuOverlay.markerBoxY(centerY, height)
                    // 1.0.288 — etichetta TX nascosta: unico indicatore TX = linea rossa animata in TX.
                    visible: false
                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: Qt.rgba(0, 0, 0, 0.70)
                        border.color: "#ff00ff"
                        border.width: 1
                    }
                    Text {
                        id: txMarkerText
                        anchors.centerIn: parent
                        text: "TX " + Math.round(waterfallDisplay.txFreq)
                        color: "#ff00ff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 4
                    anchors.bottomMargin: 2
                    visible: !spectrumGpuOverlay.cppOverlayEnabled && waterfallDisplay.autoRange
                    text: "NF:" + Math.round(waterfallDisplay.measuredFloor) + "dB"
                    color: "#646464"
                    font.family: spectrumGpuOverlay.fixedFontFamily
                    font.pixelSize: 8
                }
            }

            Rectangle {
                id: rxFilterWaterfallOverlay
                x: waterfallDisplay.rxFilterLeftX
                y: waterfallDisplay.spectrumHeight
                z: 2
                width: Math.max(0, waterfallDisplay.rxFilterRightX - waterfallDisplay.rxFilterLeftX)
                height: Math.max(0, waterfallDisplay.height - waterfallDisplay.spectrumHeight)
                visible: (bridge.monitoring || bridge.transmitting || bridge.tuning) && width > 0 && height > 0
                color: Qt.rgba(0.31, 0.43, 0.47, 0.10)
                border.color: Qt.rgba(0.55, 0.80, 0.90, 0.16)
                border.width: 1
            }

            // 1.0.288 — Linea TX ROSSA animata NELLA CASCATA (al centro della portante TX),
            // visibile SOLO in trasmissione/tune. Sostituisce la vecchia guida magenta
            // (nascosta come nello spettro). Solo visuale, niente mouse.
            Item {
                id: txSignalWaterfallGuide
                x: 0
                y: waterfallDisplay.spectrumHeight
                z: 3
                width: waterfallDisplay.width
                height: Math.max(0, waterfallDisplay.height - waterfallDisplay.spectrumHeight)
                readonly property real markerX: waterfallDisplay.freqToPixel(waterfallDisplay.txFreq)
                visible: bridge && (bridge.transmitting || bridge.tuning)
                         && height > 0 && markerX >= 0 && markerX < width

                // 1.0.288 — onda rossa che si propaga lateralmente dal centro e svanisce
                // (effetto evanescente "come se la riga si propagasse"), in loop durante il TX.
                Rectangle {
                    id: txCascadeWave
                    y: 0
                    height: parent.height
                    radius: 3
                    color: "#ff3030"
                    x: txSignalWaterfallGuide.markerX - width / 2
                    ParallelAnimation {
                        running: txSignalWaterfallGuide.visible
                        loops: Animation.Infinite
                        NumberAnimation { target: txCascadeWave; property: "width"; from: 3; to: 54; duration: 1200; easing.type: Easing.OutQuad }
                        NumberAnimation { target: txCascadeWave; property: "opacity"; from: 0.55; to: 0.0; duration: 1200; easing.type: Easing.OutQuad }
                    }
                }

                // Linea centrale rossa (portante TX)
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX) - 4; width: 9; height: parent.height; color: "#ff1414"; opacity: 0.22 }
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX) - 1; width: 3; height: parent.height; color: "#ff2020"; opacity: 0.95 }
                Rectangle { x: Math.round(txSignalWaterfallGuide.markerX);     width: 1; height: parent.height; color: "#ffd0d0" }

                // Marca "TX" in cima alla linea
                Rectangle {
                    y: 2
                    width: txCascadeBadge.implicitWidth + 10
                    height: txCascadeBadge.implicitHeight + 4
                    radius: 3
                    x: Math.max(0, Math.min(parent.width - width,
                               Math.round(txSignalWaterfallGuide.markerX) - width / 2))
                    color: Qt.rgba(0.85, 0.05, 0.05, 0.95)
                    border.color: "#ffffff"
                    border.width: 1
                    Text {
                        id: txCascadeBadge
                        anchors.centerIn: parent
                        text: "TX"
                        color: "#ffffff"
                        font.pixelSize: 9
                        font.bold: true
                    }
                }
            }

            // Overlay "Start monitoring"
            Rectangle {
                anchors.centerIn: parent
                width: startText.width + 40; height: startText.height + 20
                color: Qt.rgba(0, 0, 0, 0.85)
                border.color: accentCyan; border.width: 1; radius: 6
                visible: !bridge.monitoring && !bridge.transmitting && !bridge.tuning
                Text {
                    id: startText
                    anchors.centerIn: parent
                    text: "4096-bin panadapter · SmartSDR style\nClick MONITOR to start"
                    font.pixelSize: 12
                    color: "#B4B4B4"
                    horizontalAlignment: Text.AlignHCenter
                }
            }
	        }
	    }

    Rectangle {
        id: waterfallControlsToggle
        visible: waterfallPanel.showControls && !waterfallPanel.controlsExpanded
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 72
        width: Math.min(136, Math.max(112, parent.width - 88))
        height: 30
        z: 200
        radius: 7
        color: toggleMouse.containsMouse
               ? Qt.rgba(accentCyan.r, accentCyan.g, accentCyan.b, 0.30)
               : Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.90)
        border.color: accentCyan
        border.width: toggleMouse.containsMouse ? 2 : 1

        Row {
            anchors.centerIn: parent
            spacing: 7
            Text {
                text: "\u2630"
                color: accentCyan
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                text: qsTr("Show controls")
                color: accentCyan
                font.pixelSize: 11
                font.bold: true
            }
        }

        MouseArea {
            id: toggleMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: waterfallPanel.setControlsExpanded(!waterfallPanel.controlsExpanded)
        }

        ToolTip.visible: toggleMouse.containsMouse
        ToolTip.delay: 450
        ToolTip.text: qsTr("Mostra i controlli della cascata")
    }

    // Sync Bridge → PanadapterItem
    Connections {
        target: bridge

        // Alta risoluzione FFTW 4096 bin — include range frequenze esatto
        function onPanadapterDataReady(dbValues, minDb, maxDb, freqMinHz, freqMaxHz) {
            if (!waterfallPanel.visible) return
            waterfallDisplay.addSpectrumData(dbValues, minDb, maxDb, freqMinHz, freqMaxHz)
        }
        // Fallback: valori normalizzati 0-1 dal legacy timer
        function onSpectrumDataReady(data) {
            if (!waterfallPanel.visible) return
            if (!bridge.monitoring) return
            waterfallDisplay.addSpectrumDataNorm(data)
        }
        function onRxFrequencyChanged() { waterfallDisplay.rxFreq = bridge.rxFrequency }
        function onTxFrequencyChanged() { waterfallDisplay.txFreq = bridge.txFrequency }
        function onSettingValueChanged(key, value) {
            waterfallPanel.restoringSettings = true
            if (key === "uiPaletteIndex") {
                waterfallPanel.setPaletteIndex(value, false)
            } else if (key === "uiWaterfallControlsExpanded") {
                waterfallPanel.controlsExpanded = waterfallPanel.coerceBool(value, true)
            } else if (key === "uiSpectrumHeight") {
                waterfallPanel.spectrumHeight = Math.round(waterfallPanel.clampNumber(value, waterfallPanel.spectrumMinHeight, waterfallPanel.spectrumMaxHeight, waterfallPanel.spectrumHeight))
            } else if (key === "uiWaterfallAutoRange") {
                autoRangeCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.autoRange = autoRangeCheck.checked
            } else if (key === "uiWaterfallShowTxBrackets") {
                txBracketsCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.showTxBrackets = txBracketsCheck.checked
            } else if (key === "uiWaterfallShowDxCluster") {
                dxClusterCheck.checked = waterfallPanel.coerceBool(value, false)
                waterfallDisplay.showDxClusterSpots = dxClusterCheck.checked
            } else if (key === "uiWaterfallPeakHold") {
                peakHoldCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.peakHold = peakHoldCheck.checked
            } else if (key === "uiWaterfallBlackLevel") {
                blackSlider.value = waterfallPanel.clampNumber(value, blackSlider.from, blackSlider.to, blackSlider.value)
                waterfallDisplay.blackLevel = blackSlider.value
            } else if (key === "uiWaterfallColorGain") {
                gainSlider.value = waterfallPanel.clampNumber(value, gainSlider.from, gainSlider.to, gainSlider.value)
                waterfallDisplay.colorGain = gainSlider.value
            } else if (key === "uiWaterfallContrast") {
                contrastSlider.value = waterfallPanel.clampNumber(value, contrastSlider.from, contrastSlider.to, contrastSlider.value)
                waterfallDisplay.contrastLevel = contrastSlider.value
                waterfallPanel.applyManualContrast()
            } else if (key === "spectrumInterval") {
                speedSlider.value = waterfallPanel.clampNumber(value, speedSlider.from, speedSlider.to, speedSlider.value)
            } else if (key === "uiZoomFactor") {
                zoomSlider.value = waterfallPanel.clampNumber(value, zoomSlider.from, zoomSlider.to, zoomSlider.value)
                waterfallDisplay.zoomFactor = zoomSlider.value
            } else if (key === "uiLabelFontSize") {
                labelFontSlider.value = waterfallPanel.clampNumber(value, labelFontSlider.from, labelFontSlider.to, labelFontSlider.value)
                waterfallDisplay.labelFontSize = labelFontSlider.value
            } else if (key === "uiLabelSpacing") {
                labelSpacingSlider.value = waterfallPanel.clampNumber(value, labelSpacingSlider.from, labelSpacingSlider.to, labelSpacingSlider.value)
                waterfallDisplay.labelSpacing = labelSpacingSlider.value
            } else if (key === "uiLabelBold") {
                labelBoldCheck.checked = waterfallPanel.coerceBool(value, true)
                waterfallDisplay.labelBold = labelBoldCheck.checked
            } else if (key === "uiLabelColorPreset") {
                labelColorCombo.currentIndex = waterfallPanel.clampIndex(value, waterfallPanel.labelColorPresets.length, labelColorCombo.currentIndex)
                var preset = waterfallPanel.labelColorPresets[labelColorCombo.currentIndex]
                waterfallDisplay.labelUseCustomColor = preset.custom
                waterfallDisplay.labelColor = preset.color
            } else if (key === "uiWaterfallShowCallsigns") {
                waterfallPanel.setShowDecodeCallsigns(waterfallPanel.coerceBool(value, true), false)
            } else {
                waterfallPanel.restoringSettings = false
                return
            }
            waterfallPanel.restoringSettings = false
        }

        // Aggiorna i callsign decodificati sul grafico spettro
        function onDecodeListChanged() {
            waterfallPanel.refreshDecodeLabels()
        }
    }
}
