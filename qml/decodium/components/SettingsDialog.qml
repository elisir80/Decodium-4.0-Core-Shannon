/* Decodium 4.0 — Impostazioni Generali
 * Sostituisce il dialogo impostazioni legacy WSJT-X.
 * Tutte le modifiche sono LIVE (bind diretto alle proprieta bridge).
 * Layout orizzontale: sidebar + StackLayout con GridLayout 4 colonne.
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: settingsDialog
    // 1.0.412 — richiesta di schermo intero gestita da Main.qml (mainWindow non è in scope qui).
    signal fullScreenRequested()
    readonly property int popupViewportMargin: 16
    readonly property int popupBaseWidth: (parent && parent.width > 0) ? Math.round(parent.width) : 1440
    readonly property int popupBaseHeight: (parent && parent.height > 0) ? Math.round(parent.height) : 960
    readonly property int popupMaxWidth: Math.max(1, popupBaseWidth - popupViewportMargin)
    readonly property int popupMaxHeight: Math.max(1, popupBaseHeight - popupViewportMargin)
    readonly property bool compactSettingsLayout: width < 1180
    title: qsTr("Settings")
    modal: !warmupInProgress
    opacity: warmupInProgress ? 0 : 1
    width: Math.min(Math.max(360, Math.round(popupBaseWidth * 0.998)), popupMaxWidth, 2200)
    height: Math.min(Math.max(320, Math.round(popupBaseHeight * 0.98)), popupMaxHeight, 1080)
    closePolicy: Popup.CloseOnEscape
    property bool positionInitialized: false
    property bool warmupInProgress: false
    property int currentTab: {
        var savedTab = Number(bridge.getSetting("uiSettingsCurrentTab", 0))
        return isFinite(savedTab) ? Math.max(0, Math.min(12, Math.floor(savedTab))) : 0
    }
    property bool closeAlreadyPersisted: false
    readonly property int labelWidth: compactSettingsLayout ? 132 : 172
    readonly property int fieldMinWidth: compactSettingsLayout ? 240 : 380
    readonly property int wideFieldMinWidth: compactSettingsLayout ? 340 : 620
    readonly property int portFieldMinWidth: compactSettingsLayout ? 180 : 270
    readonly property int numericFieldMinWidth: compactSettingsLayout ? 160 : 220
    readonly property int comboFieldMinWidth: compactSettingsLayout ? 240 : 320
    readonly property int frequencyPageMinWidth: compactSettingsLayout ? 900 : 1120
    readonly property int scrollLeftMargin: 10
    readonly property int scrollTopMargin: 10
    readonly property int scrollRightMargin: 12
    readonly property int scrollBottomMargin: 96
    property string dataDownloadStatus: ""
    property bool dataDownloadIsError: false
    property string uiFontLabel: bridge.fontSettingLabel("Font", "", 0)
    property string decodedFontLabel: bridge.fontSettingLabel("DecodedTextFont", "Courier", 10)
    property string fontPickerKey: ""
    property string fontPickerFallbackFamily: ""
    property int fontPickerFallbackPointSize: 0
    property string fontPickerFamily: ""
    property int fontPickerPointSize: 10
    property bool fontPickerBold: false
    property bool fontPickerItalic: false
    property bool fontPickerFixedOnly: false
    property string fontPickerSearch: ""
    property var fontPickerFamilies: []
    property bool loggingChecksUpdating: false
    property var workingFrequencyRows: []
    property var stationFrequencyRows: []
    property var frequencyRegionOptions: bridge.frequencyRegionOptions()
    property var frequencyModeOptions: bridge.frequencyModeOptions()
    property var frequencyBandOptions: bridge.frequencyBandOptions()
    property int selectedWorkingFrequencyIndex: -1
    property int selectedStationFrequencyIndex: -1
    property string qrzLogbookTestStatus: ""
    property bool qrzLogbookTestIsError: false
    property bool qrzLogbookTestBusy: false

    function refreshFontLabels() {
        uiFontLabel = bridge.fontSettingLabel("Font", "", 0)
        decodedFontLabel = bridge.fontSettingLabel("DecodedTextFont", "Courier", 10)
    }

    function boolSetting(key, fallback) {
        var value = bridge.getSetting(key, fallback)
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

    function setBoolSettingIfChanged(key, value, fallback) {
        if (boolSetting(key, fallback) !== value)
            bridge.setSetting(key, value)
    }

    function territorySettingMatches(key, code, aliases) {
        var raw = String(bridge.getSetting(key, "") || "")
        if (raw.trim().length === 0)
            return false

        var text = raw.toUpperCase().replace(/[^A-Z]+/g, " ").trim()
        var compact = text.replace(/\s+/g, "")
        var accepted = [code].concat(aliases || [])
        for (var i = 0; i < accepted.length; ++i) {
            var alias = String(accepted[i] || "").toUpperCase().replace(/[^A-Z]+/g, " ").trim()
            if (alias.length === 0)
                continue
            var aliasCompact = alias.replace(/\s+/g, "")
            if (text === alias || compact === aliasCompact || text.indexOf(alias) >= 0)
                return true
        }
        return false
    }

    function normalizeTerritorySetting(key, code, aliases) {
        var raw = String(bridge.getSetting(key, "") || "")
        if (raw.trim().length === 0)
            return
        bridge.setSetting(key, territorySettingMatches(key, code, aliases) ? code : "")
    }

    function setTerritoryExcluded(key, code, excluded) {
        var value = excluded ? code : ""
        if (String(bridge.getSetting(key, "") || "") !== value)
            bridge.setSetting(key, value)
    }

    // 1.0.306 (#4) — config "bande operative": lista completa (lambda + etichetta) e helper
    // per il setting "uiDisabledBands" (CSV di lambda nascosti dal selettore). Vuoto = tutte.
    readonly property var allBandsForConfig: [
        { l: "160M", n: "1.8" }, { l: "80M", n: "3.5" }, { l: "60M", n: "5" }, { l: "40M", n: "7" },
        { l: "30M", n: "10" }, { l: "20M", n: "14" }, { l: "17M", n: "18" }, { l: "15M", n: "21" },
        { l: "12M", n: "24" }, { l: "10M", n: "28" }, { l: "8M", n: "40" }, { l: "6M", n: "50" },
        { l: "4M", n: "70" }, { l: "2M", n: "144" }, { l: "1.25M", n: "222" }, { l: "70CM", n: "432" },
        { l: "33CM", n: "902" }, { l: "23CM", n: "1296" }, { l: "13CM", n: "2304" }, { l: "9CM", n: "3400" },
        { l: "6CM", n: "5760" }, { l: "3CM", n: "10G" }, { l: "1.25CM", n: "24G" }
    ]
    property string disabledBandsCsv: bridge ? String(bridge.getSetting("uiDisabledBands", "") || "") : ""
    function bandEnabledCfg(lambda) {
        return ("," + disabledBandsCsv + ",").indexOf("," + lambda + ",") < 0
    }
    function toggleBandCfg(lambda, enable) {
        var set = disabledBandsCsv.length ? disabledBandsCsv.split(",").filter(function(x){ return x.length > 0 }) : []
        var idx = set.indexOf(lambda)
        if (enable) { if (idx >= 0) set.splice(idx, 1) }
        else        { if (idx <  0) set.push(lambda) }
        var csv = set.join(",")
        if (bridge) bridge.setSetting("uiDisabledBands", csv)
        disabledBandsCsv = csv
    }

    function setLoggingMode(promptMode) {
        if (loggingChecksUpdating)
            return

        loggingChecksUpdating = true
        promptToLogCheck.checked = promptMode
        autoLogCheck.checked = !promptMode
        bridge.setSetting("PromptToLog", promptMode)
        bridge.setSetting("AutoLog", !promptMode)
        loggingChecksUpdating = false
    }

    function normalizeLoggingModeChecks() {
        loggingChecksUpdating = true
        var promptMode = boolSetting("PromptToLog", false)
        var autoMode = boolSetting("AutoLog", true)
        if (promptMode === autoMode) {
            promptMode = false
            autoMode = true
            bridge.setSetting("PromptToLog", false)
            bridge.setSetting("AutoLog", true)
        }
        promptToLogCheck.checked = promptMode
        autoLogCheck.checked = autoMode
        loggingChecksUpdating = false
    }

    function updateQrzLogbookTestStatus(msg, isError) {
        var text = String(msg || "")
        var lower = text.toLowerCase()
        if (lower.indexOf("qrz logbook:") !== 0)
            return false

        qrzLogbookTestStatus = text.replace(/^QRZ Logbook:\s*/i, "")
        qrzLogbookTestIsError = isError
        qrzLogbookTestBusy = !isError
                && (lower.indexOf("test in corso") >= 0
                    || lower.indexOf("testing") >= 0)
        return true
    }

    function refreshFrequencySettings() {
        workingFrequencyRows = bridge.workingFrequencyRows()
        stationFrequencyRows = bridge.stationFrequencyRows()
        frequencyRegionOptions = bridge.frequencyRegionOptions()
        frequencyModeOptions = bridge.frequencyModeOptions()
        frequencyBandOptions = bridge.frequencyBandOptions()
        if (selectedWorkingFrequencyIndex >= workingFrequencyRows.length)
            selectedWorkingFrequencyIndex = -1
        if (selectedStationFrequencyIndex >= stationFrequencyRows.length)
            selectedStationFrequencyIndex = -1
        if (typeof frequencySlopeField !== "undefined")
            frequencySlopeField.text = Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
        if (typeof frequencyInterceptField !== "undefined")
            frequencyInterceptField.text = Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
    }

    function commitFrequencySlope(text) {
        var value = Number(String(text).replace(",", "."))
        if (!isFinite(value))
            value = bridge.frequencyCalibrationSlopePpm()
        bridge.setFrequencyCalibrationSlopePpm(value)
        return Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
    }

    function commitFrequencyIntercept(text) {
        var value = Number(String(text).replace(",", "."))
        if (!isFinite(value))
            value = bridge.frequencyCalibrationInterceptHz()
        bridge.setFrequencyCalibrationInterceptHz(value)
        return Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
    }

    function setComboText(combo, value) {
        if (!combo)
            return
        var text = String(value || "")
        for (var i = 0; i < combo.count; ++i) {
            if (combo.textAt(i) === text) {
                combo.currentIndex = i
                return
            }
        }
        combo.currentIndex = combo.count > 0 ? 0 : -1
    }

    function selectWorkingFrequencyRow(row) {
        if (!row)
            return
        selectedWorkingFrequencyIndex = Number(row.index)
        setComboText(frequencyRegionCombo, row.region || "All")
        setComboText(frequencyModeCombo, row.mode || "FT8")
        frequencyMHzField.text = row.frequencyMHz || ""
        frequencyPreferredCheck.checked = !!row.preferred
        frequencyDescriptionField.text = row.description || ""
        frequencyStartField.text = row.startTime || ""
        frequencyEndField.text = row.endTime || ""
    }

    function clearWorkingFrequencyEditor() {
        selectedWorkingFrequencyIndex = -1
        setComboText(frequencyRegionCombo, "All")
        setComboText(frequencyModeCombo, bridge.mode || "FT8")
        frequencyMHzField.text = ""
        frequencyPreferredCheck.checked = false
        frequencyDescriptionField.text = ""
        frequencyStartField.text = ""
        frequencyEndField.text = ""
    }

    function workingFrequencyEditorFrequencyText() {
        var text = String(frequencyMHzField.text || "").trim()
        var lower = text.toLowerCase()
        var explicitMHz = lower.indexOf("mhz") >= 0
        var explicitHz = lower.indexOf("hz") >= 0 && !explicitMHz
        text = text.replace(/,/g, ".")
        text = text.replace(/mhz/ig, "")
        text = text.replace(/hz/ig, "")
        text = text.replace(/\s+/g, "")
        if (text.length === 0)
            return ""
        return text + (explicitMHz ? " MHz" : (explicitHz ? " Hz" : ""))
    }

    function workingFrequencyEditorHasValidFrequency() {
        var text = workingFrequencyEditorFrequencyText()
        if (text.length === 0)
            return false
        var numeric = Number(text.replace(/mhz|hz/ig, "").trim())
        return isFinite(numeric) && numeric > 0
    }

    function newWorkingFrequencyEditor() {
        selectedWorkingFrequencyIndex = -1
        setComboText(frequencyRegionCombo, "All")
        setComboText(frequencyModeCombo, bridge.mode || "FT8")
        var currentHz = Number(bridge.frequency) || 0
        frequencyMHzField.text = currentHz > 0 ? (currentHz / 1000000.0).toFixed(6) : ""
        frequencyPreferredCheck.checked = true
        frequencyDescriptionField.text = ""
        frequencyStartField.text = ""
        frequencyEndField.text = ""
        Qt.callLater(function() {
            frequencyMHzField.forceActiveFocus()
            frequencyMHzField.selectAll()
        })
    }

    function addWorkingFrequencyFromEditor() {
        if (!workingFrequencyEditorHasValidFrequency())
            return
        if (bridge.addWorkingFrequencyRow(frequencyRegionCombo.currentText,
                                          frequencyModeCombo.currentText,
                                          workingFrequencyEditorFrequencyText(),
                                          frequencyDescriptionField.text,
                                          frequencyStartField.text,
                                          frequencyEndField.text,
                                          frequencyPreferredCheck.checked)) {
            refreshFrequencySettings()
        }
    }

    function updateWorkingFrequencyFromEditor() {
        if (selectedWorkingFrequencyIndex < 0)
            return
        if (!workingFrequencyEditorHasValidFrequency())
            return
        if (bridge.updateWorkingFrequencyRow(selectedWorkingFrequencyIndex,
                                             frequencyRegionCombo.currentText,
                                             frequencyModeCombo.currentText,
                                             workingFrequencyEditorFrequencyText(),
                                             frequencyDescriptionField.text,
                                             frequencyStartField.text,
                                             frequencyEndField.text,
                                             frequencyPreferredCheck.checked)) {
            refreshFrequencySettings()
        }
    }

    function deleteSelectedWorkingFrequency() {
        if (selectedWorkingFrequencyIndex < 0)
            return
        if (bridge.deleteWorkingFrequencyRow(selectedWorkingFrequencyIndex)) {
            clearWorkingFrequencyEditor()
            refreshFrequencySettings()
        }
    }

    function selectStationFrequencyRow(row) {
        if (!row)
            return
        selectedStationFrequencyIndex = Number(row.index)
        setComboText(stationBandCombo, row.band || "20m")
        stationOffsetField.text = row.offsetMHz || String(row.offset || "").replace(" MHz", "")
        stationAntennaField.text = row.antenna || ""
    }

    function clearStationFrequencyEditor() {
        selectedStationFrequencyIndex = -1
        setComboText(stationBandCombo, "20m")
        stationOffsetField.text = "0.000000"
        stationAntennaField.text = ""
    }

    function addStationFrequencyFromEditor() {
        if (bridge.addStationFrequencyRow(stationBandCombo.currentText,
                                          stationOffsetField.text,
                                          stationAntennaField.text)) {
            refreshFrequencySettings()
        }
    }

    function updateStationFrequencyFromEditor() {
        if (selectedStationFrequencyIndex < 0)
            return
        if (bridge.updateStationFrequencyRow(selectedStationFrequencyIndex,
                                             stationBandCombo.currentText,
                                             stationOffsetField.text,
                                             stationAntennaField.text)) {
            refreshFrequencySettings()
        }
    }

    function deleteSelectedStationFrequency() {
        if (selectedStationFrequencyIndex < 0)
            return
        if (bridge.deleteStationFrequencyRow(selectedStationFrequencyIndex)) {
            clearStationFrequencyEditor()
            refreshFrequencySettings()
        }
    }

    function normalizedHexColor(value) {
        var text = String(value || "").trim()
        if (text.length === 6 && text.charAt(0) !== "#")
            text = "#" + text
        return text.toUpperCase()
    }

    function validHexColor(value) {
        return /^#[0-9A-Fa-f]{6}$/.test(String(value || "").trim())
    }

    function setDecodeHighlightColor(prop, value) {
        var normalized = normalizedHexColor(value)
        if (!validHexColor(normalized))
            return false

        bridge[prop] = normalized
        bridge.setSetting(prop, normalized)
        bridge.saveSettings()
        return true
    }

    component SettingsComboPopup: Popup {
        id: comboPopup
        property var combo: null
        property int minPopupWidth: 220
        property int maxPopupHeight: 360
        readonly property var comboOrigin: combo && parent ? combo.mapToItem(parent, 0, 0) : Qt.point(0, 0)
        readonly property real wantedHeight: Math.min(maxPopupHeight, comboPopupList.contentHeight + padding * 2)
        readonly property real spaceBelow: parent && combo ? parent.height - comboOrigin.y - combo.height - 8 : maxPopupHeight
        readonly property real spaceAbove: parent && combo ? comboOrigin.y - 8 : 0
        readonly property bool openAbove: wantedHeight > spaceBelow && spaceAbove > spaceBelow

        parent: Overlay.overlay
        modal: false
        focus: true
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: parent ? Math.min(Math.max(combo ? combo.width : 0, minPopupWidth), Math.max(80, parent.width - 16))
                      : Math.max(combo ? combo.width : 0, minPopupWidth)
        height: Math.max(44, Math.min(wantedHeight, Math.max(44, openAbove ? spaceAbove : spaceBelow)))
        x: parent ? Math.max(8, Math.min(comboOrigin.x, parent.width - width - 8)) : 0
        y: parent
           ? (openAbove
              ? Math.max(8, comboOrigin.y - height - 2)
              : Math.min(comboOrigin.y + (combo ? combo.height : 0) + 2, parent.height - height - 8))
           : 0
        onOpened: comboPopupList.forceActiveFocus()

        background: Rectangle {
            color: bgDeep
            border.color: glassBorder
            radius: 4
        }

        contentItem: ListView {
            id: comboPopupList
            anchors.fill: parent
            clip: true
            model: comboPopup.visible && combo ? combo.delegateModel : null
            currentIndex: combo ? combo.highlightedIndex : -1
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            interactive: true
            focus: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            function clampContentY(value) {
                return Math.max(0, Math.min(Math.max(0, contentHeight - height), value))
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: function(wheel) {
                    var pixelDelta = wheel.pixelDelta ? wheel.pixelDelta.y : 0
                    var angleDelta = wheel.angleDelta ? wheel.angleDelta.y : 0
                    var step = pixelDelta !== 0 ? pixelDelta : angleDelta / 120 * 48
                    if (step === 0)
                        return

                    comboPopupList.contentY = comboPopupList.clampContentY(comboPopupList.contentY - step)
                    wheel.accepted = true
                }
            }
        }
    }

    function setAlertEnabled(value) {
        bridge.alertSoundsEnabled = value
        bridge.setSetting("alertSoundsEnabled", value)
        bridge.setSetting("alert_Enabled", value)
    }

    function setAlertCq(value) {
        bridge.alertOnCq = value
        bridge.setSetting("alertOnCq", value)
        bridge.setSetting("alert_CQ", value)
    }

    function setAlertMyCall(value) {
        bridge.alertOnMyCall = value
        bridge.setSetting("alertOnMyCall", value)
        bridge.setSetting("alert_MyCall", value)
    }

    function localDirectoryToUrl(path) {
        var text = String(path || "").trim()
        if (text.length === 0)
            return ""
        if (text.indexOf("file:") === 0)
            return text
        text = text.replace(/\\/g, "/")
        if (Qt.platform.os === "windows" && /^[A-Za-z]:\//.test(text))
            return "file:///" + encodeURI(text)
        if (text.charAt(0) === "/")
            return "file://" + encodeURI(text)
        return "file:///" + encodeURI(text)
    }

    function folderUrlToLocalDirectory(url) {
        var text = String(url || "")
        if (text.indexOf("file:///") === 0) {
            var absolutePath = decodeURIComponent(text.substring(8))
            return Qt.platform.os === "windows" ? absolutePath : "/" + absolutePath
        }
        if (text.indexOf("file://") === 0)
            return decodeURIComponent(text.substring(7))
        return decodeURIComponent(text)
    }

    function fileUrlToLocalPath(url) {
        return folderUrlToLocalDirectory(url)
    }

    function openDirectoryPicker(settingKey, currentPath) {
        var title = settingKey === "AzElDirectory" ? qsTr("Select AzEl directory") : qsTr("Select save directory")
        var path = bridge.openDirectoryDialog(title, currentPath)
        if (settingKey === "" || path === "")
            return
        bridge.setSetting(settingKey, path)
        if (settingKey === "SaveDirectory")
            saveDirectoryField.text = path
        else if (settingKey === "AzElDirectory")
            azElDirectoryField.text = path
    }

    function openWorkingFrequenciesLoadDialog(mergeMode) {
        var path = bridge.openFileDialog(mergeMode ? qsTr("Merge Working Frequencies") : qsTr("Load Working Frequencies"),
                                         "",
                                         [qsTr("Frequency files (*.qrg *.qrg.json)"), qsTr("All files (*)")])
        if (path.length > 0 && bridge.loadWorkingFrequenciesFile(path, mergeMode)) {
            settingsDialog.clearWorkingFrequencyEditor()
            settingsDialog.refreshFrequencySettings()
        }
    }

    function openWorkingFrequenciesSaveDialog() {
        var path = bridge.saveFileDialog(qsTr("Save Working Frequencies"),
                                         "",
                                         [qsTr("Frequency files (*.qrg *.qrg.json)"), qsTr("All files (*)")])
        if (path.length > 0)
            bridge.saveWorkingFrequenciesFile(path)
    }

    Connections {
        target: bridge
        function onSettingValueChanged(key, value) {
            settingsDialog.scheduleSettingsPersist()
            if (key === "Font" || key === "DecodedTextFont")
                settingsDialog.refreshFontLabels()
        }
        function onStatusMessage(msg) {
            var text = String(msg || "")
            var lower = text.toLowerCase()
            if (settingsDialog.updateQrzLogbookTestStatus(text, false))
                return
            if (lower.indexOf("cty.dat") >= 0 || lower.indexOf("call3.txt") >= 0) {
                dataDownloadStatus = text
                dataDownloadIsError = false
            }
        }
        function onErrorMessage(msg) {
            var text = String(msg || "")
            var lower = text.toLowerCase()
            if (settingsDialog.updateQrzLogbookTestStatus(text, true))
                return
            if (lower.indexOf("cty.dat") >= 0 || lower.indexOf("call3.txt") >= 0) {
                dataDownloadStatus = text
                dataDownloadIsError = true
            }
        }
        function onActiveCatProfileChanged() {
            settingsDialog.refreshCatProfileDraft()
        }
        function onCatProfilesChanged() {
            settingsDialog.refreshCatProfileDraft()
        }
    }

    function activeCatController() {
        return bridge.catManager ? bridge.catManager : null
    }

    function catConnectionInProgress() {
        var controller = activeCatController()
        return !!(controller && controller.connecting)
    }

    function activeCatPortType() {
        var controller = activeCatController()
        if (!controller || controller.portType === undefined || controller.portType === null)
            return "none"
        return String(controller.portType)
    }

    function activeRigName() {
        var controller = activeCatController()
        if (!controller || controller.rigName === undefined || controller.rigName === null)
            return ""
        return String(controller.rigName)
    }

    function activeBaudRateText() {
        var controller = activeCatController()
        if (!controller || controller.baudRate === undefined || controller.baudRate === null)
            return ""
        var text = String(controller.baudRate).trim()
        return text === "0" ? "" : text
    }

    function activeStopBitsText() {
        var controller = activeCatController()
        if (!controller || controller.stopBits === undefined || controller.stopBits === null)
            return "Default"
        var text = String(controller.stopBits).trim().toLowerCase()
        if (text === "" || text === "default" || text === "predefinito" || text === "auto")
            return "Default"
        if (text === "2" || text === "2.0" || text.indexOf("two") >= 0)
            return "2"
        if (text === "1" || text === "1.0" || text.indexOf("one") >= 0)
            return "1"
        return "Default"
    }

    function normalizedCatSerialChoice(value) {
        var text = String(value === undefined || value === null ? "" : value).trim()
        var lower = text.toLowerCase()
        if (lower === "" || lower === "default" || lower === "predefinito" || lower === "auto")
            return "Default"
        if (lower === "none" || lower === "no" || lower === "off")
            return "none"
        if (lower === "xonxoff" || lower === "xon/xoff" || lower.indexOf("software") >= 0)
            return "xonxoff"
        if (lower === "hardware" || lower === "hw")
            return "hardware"
        if (lower === "7" || lower.indexOf("seven") >= 0)
            return "7"
        if (lower === "8" || lower.indexOf("eight") >= 0)
            return "8"
        if (lower === "2" || lower === "2.0" || lower.indexOf("two") >= 0)
            return "2"
        if (lower === "1" || lower === "1.0" || lower.indexOf("one") >= 0)
            return "1"
        return "Default"
    }

    function catSerialChoiceIndex(model, value, fallbackIndex) {
        var wanted = normalizedCatSerialChoice(value).toLowerCase()
        for (var i = 0; i < model.length; ++i) {
            if (String(model[i]).trim().toLowerCase() === wanted)
                return i
        }
        return fallbackIndex
    }

    function handshakeChoiceLabel(value) {
        var normalized = normalizedCatSerialChoice(value)
        if (normalized === "Default")
            return qsTr("Default")
        if (normalized === "none")
            return qsTr("None")
        if (normalized === "xonxoff")
            return "XON/XOFF"
        if (normalized === "hardware")
            return qsTr("Hardware")
        return String(value)
    }

    function stringListIndexOf(list, value) {
        if (!list || value === undefined || value === null)
            return -1
        var wanted = String(value)
        var wantedNorm = wanted.trim().toLowerCase()
        for (var i = 0; i < list.length; ++i) {
            var candidate = String(list[i])
            var candidateNorm = candidate.trim().toLowerCase()
            if (candidate === wanted
                    || candidateNorm === wantedNorm
                    || candidateNorm.indexOf(wantedNorm) !== -1
                    || wantedNorm.indexOf(candidateNorm) !== -1)
                return i
        }
        return -1
    }

    function selectTciRigIfNeeded() {
        var controller = activeCatController()
        if (!controller || controller.rigName === undefined || controller.rigName === null)
            return
        var currentRig = String(controller.rigName || "")
        if (controller.pttMethod !== undefined)
            controller.pttMethod = "CAT"
        if (currentRig.indexOf("TCI Client") === 0)
            return
        var rigs = controller.rigList || []
        var rx1Index = stringListIndexOf(rigs, "TCI Client RX1")
        controller.rigName = rx1Index >= 0 ? String(rigs[rx1Index]) : "TCI Client RX1"
    }

    function normalizedRigName(value) {
        return String(value || "").toUpperCase().replace(/[\s_]+/g, "")
    }

    function rigIsIcom() {
        var rig = normalizedRigName(activeRigName())
        return rig.indexOf("ICOM") !== -1 || rig.indexOf("IC-") !== -1 || rig.indexOf("IC7") !== -1
                || rig.indexOf("IC9") !== -1 || rig.indexOf("IC705") !== -1
    }

    function civAddressText() {
        var controller = activeCatController()
        if (!controller || controller.civAddress === undefined || controller.civAddress === null)
            return ""
        var v = Number(controller.civAddress)
        if (!isFinite(v) || v <= 0)
            return ""
        v = Math.max(0, Math.min(255, Math.round(v)))
        return "0x" + v.toString(16).toUpperCase().padStart(2, "0")
    }

    function civAddressPlaceholderText() {
        var rig = normalizedRigName(activeRigName()).replace(/-/g, "")
        if (rig.indexOf("IC7300MK2") !== -1)
            return "0xB6 (IC-7300MK2)"
        if (rig.indexOf("IC7300") !== -1)
            return "0x94 (IC-7300)"
        return qsTr("Auto")
    }

    function usesSerialControls() {
        var portType = activeCatPortType()
        return portType === "serial" || portType === "usb"
    }

    function usesNetworkControls() {
        return activeCatPortType() === "network"
    }

    function usesTciControls() {
        return bridge.catBackend === "tci" || activeCatPortType() === "tci"
    }

    function activePttMethod() {
        var controller = activeCatController()
        if (!controller || controller.pttMethod === undefined || controller.pttMethod === null)
            return "CAT"
        var method = String(controller.pttMethod).trim().toUpperCase()
        return method === "" ? "CAT" : method
    }

    function usesSeparatePttPort() {
        var method = activePttMethod()
        return method === "DTR" || method === "RTS"
    }

    function pttPortOptions() {
        var controller = activeCatController()
        var options = []
        if (!controller || !usesSeparatePttPort())
            return options

        if (activeCatPortType() === "serial")
            options.push("CAT")

        var ports = controller.portList || []
        for (var i = 0; i < ports.length; ++i) {
            var port = String(ports[i]).trim()
            if (port !== "" && settingsDialog.stringListIndexOf(options, port) < 0)
                options.push(port)
        }

        var saved = controller.pttPort !== undefined && controller.pttPort !== null
                ? String(controller.pttPort).trim() : ""
        if (saved !== "" && saved.toUpperCase() !== "CAT"
                && settingsDialog.stringListIndexOf(options, saved) < 0)
            options.push(saved)
        return options
    }

    function normalizedPortName(value) {
        var text = String(value || "").trim()
        if (text === "" || text.toUpperCase() === "CAT")
            return "CAT"
        if (text.indexOf("/dev/") === 0)
            text = text.substring(5)
        return text.toLowerCase()
    }

    function pttSharesCatPort() {
        var controller = activeCatController()
        if (!controller)
            return false
        var pttPort = normalizedPortName(controller.pttPort)
        if (pttPort === "CAT")
            return true
        return pttPort === normalizedPortName(controller.serialPort)
    }

    function forceDtrControlEnabled() {
        return activeCatPortType() === "serial"
                && !(activePttMethod() === "DTR" && pttSharesCatPort())
    }

    function forceRtsControlEnabled() {
        return activeCatPortType() === "serial"
                && !(activePttMethod() === "RTS" && pttSharesCatPort())
    }

    function enforceForceLineAvailability() {
        var controller = activeCatController()
        if (!controller)
            return
        var changed = false
        if (!forceDtrControlEnabled() && (controller.forceDtr || controller.dtrHigh)) {
            controller.forceDtr = false
            controller.dtrHigh = false
            changed = true
        }
        if (!forceRtsControlEnabled() && (controller.forceRts || controller.rtsHigh)) {
            controller.forceRts = false
            controller.rtsHigh = false
            changed = true
        }
        if (changed)
            scheduleCatPersist()
    }

    function supportsSwrTelemetry() {
        var rig = normalizedRigName(activeRigName())
        if (bridge.catBackend === "omnirig")
            return false
        if (rig.indexOf("OMNIRIG") === 0 || rig.indexOf("DXLAB") === 0 || rig.indexOf("HAMRADIO") === 0)
            return false
        if (rig.indexOf("KENWOODTS-480") === 0 || rig.indexOf("KENWOODTS-850") === 0 || rig.indexOf("KENWOODTS-870") === 0)
            return false
        return true
    }

    function splitModeLabel(value) {
        if (value === "rig")
            return qsTr("Rig")
        if (value === "emulate")
            return qsTr("Fake It")
        return qsTr("None")
    }

    function splitModeOptions() {
        var controller = activeCatController()
        var source = controller && controller.splitModeList ? controller.splitModeList : ["none", "rig", "emulate"]
        var options = []
        for (var i = 0; i < source.length; ++i) {
            var value = String(source[i])
            options.push({ value: value, label: splitModeLabel(value) })
        }
        return options
    }

    function setupChoiceLabel(value) {
        var text = String(value)
        if (text === "None")
            return qsTr("None")
        if (text === "Default")
            return qsTr("Default")
        if (text === "On")
            return qsTr("On")
        if (text === "Off")
            return qsTr("Off")
        if (text === "Mono")
            return qsTr("Mono")
        if (text === "Left")
            return qsTr("Left")
        if (text === "Right")
            return qsTr("Right")
        if (text === "Both")
            return qsTr("Both")
        if (text === "Rear/Data")
            return qsTr("Rear/Data")
        if (text === "Front/Mic")
            return qsTr("Front/Mic")
        return text
    }

    function settingChoiceIndex(key, choices, fallbackIndex) {
        var raw = bridge.getSetting(key, fallbackIndex)
        var numeric = Number(raw)
        if (!isNaN(numeric) && numeric >= 0 && numeric < choices.length)
            return numeric

        var text = String(raw).trim().toLowerCase()
        for (var i = 0; i < choices.length; ++i) {
            if (String(choices[i]).trim().toLowerCase() === text)
                return i
        }
        return fallbackIndex
    }

    function forceLineMode(forceEnabled, highLevel) {
        if (!forceEnabled)
            return "Default"
        return highLevel ? "On" : "Off"
    }

    function applyForceLineValue(lineName, value) {
        var controller = activeCatController()
        if (!controller)
            return
        if (lineName === "dtr" && !forceDtrControlEnabled())
            value = "Default"
        if (lineName === "rts" && !forceRtsControlEnabled())
            value = "Default"
        var forceEnabled = value !== "Default"
        var highLevel = value === "On"
        if (lineName === "dtr") {
            controller.forceDtr = forceEnabled
            controller.dtrHigh = highLevel
        } else {
            controller.forceRts = forceEnabled
            controller.rtsHigh = highLevel
        }
        scheduleCatPersist()
    }

    function resetForcedSerialLines() {
        var controller = activeCatController()
        if (!controller)
            return
        controller.forceDtr = false
        controller.dtrHigh = false
        controller.forceRts = false
        controller.rtsHigh = false
        forceDtrCombo.currentIndex = 0
        forceRtsCombo.currentIndex = 0
    }

    function openTab(index) {
        var tab = Number(index)
        currentTab = isFinite(tab) ? Math.max(0, Math.min(12, Math.floor(tab))) : 0
        open()
    }

    function warmUpPopup() {
        if (visible || warmupInProgress)
            return
        warmupInProgress = true
        positionInitialized = true
        x = -width - 10000
        y = -height - 10000
        open()
        warmupCloseTimer.restart()
    }

    function toggleCatConnection() {
        var controller = activeCatController()
        if (!controller) return
        if (controller.connecting) return
        if (bridge.catConnected) controller.disconnectRig()
        else controller.connectRig()
    }

    function refreshCatPorts() {
        var controller = activeCatController()
        if (controller && controller.refreshPorts) controller.refreshPorts()
    }

    function selectedCatProfileName() {
        if (catProfileCombo && catProfileCombo.currentIndex >= 0)
            return String(catProfileCombo.currentText || "").trim()
        return String(bridge.activeCatProfile || "").trim()
    }

    function nextCatProfileName() {
        var base = String(bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : "Radio Profile").trim()
        if (base.length === 0)
            base = "Radio Profile"
        var profiles = bridge.catProfileList || []
        var exists = function(name) {
            for (var i = 0; i < profiles.length; ++i) {
                if (String(profiles[i]).trim().toLowerCase() === String(name).trim().toLowerCase())
                    return true
            }
            return false
        }
        if (!exists(base))
            return base
        for (var n = 2; n < 100; ++n) {
            var candidate = base + " " + n
            if (!exists(candidate))
                return candidate
        }
        return base + " " + Date.now()
    }

    function refreshCatProfileDraft() {
        if (!catProfileNameField)
            return
        var active = String(bridge.activeCatProfile || "").trim()
        if (active.length > 0) {
            catProfileNameField.text = active
        } else if (String(catProfileNameField.text || "").trim().length === 0) {
            catProfileNameField.text = bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : ""
        }
    }

    function saveCatProfileFromField() {
        var name = String(catProfileNameField.text || "").trim()
        if (name.length === 0)
            name = nextCatProfileName()
        if (bridge.saveCatProfile(name))
            catProfileNameField.text = String(bridge.activeCatProfile || name)
    }

    function saveNewCatProfileFromCurrent() {
        catProfileNameField.text = nextCatProfileName()
        saveCatProfileFromField()
    }

    function loadSelectedCatProfile() {
        var name = selectedCatProfileName()
        if (name.length === 0)
            return
        if (bridge.loadCatProfile(name)) {
            catProfileNameField.text = String(bridge.activeCatProfile || name)
            refreshCatPorts()
        }
    }

    function deleteSelectedCatProfile() {
        var name = selectedCatProfileName()
        if (name.length === 0)
            return
        if (bridge.deleteCatProfile(name))
            catProfileNameField.text = bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : ""
    }

    function refreshAudioDevices() {
        if (bridge && bridge.refreshAudioDevices)
            bridge.refreshAudioDevices()
    }

    function filteredFontFamilies() {
        var filter = String(fontPickerSearch || "").trim().toLowerCase()
        if (filter === "")
            return fontPickerFamilies
        var result = []
        for (var i = 0; i < fontPickerFamilies.length; ++i) {
            var family = String(fontPickerFamilies[i])
            if (family.toLowerCase().indexOf(filter) !== -1)
                result.push(family)
        }
        return result
    }

    function openFontPicker(key, fallbackFamily, fallbackPointSize, fixedOnly) {
        fontPickerKey = key
        fontPickerFallbackFamily = fallbackFamily
        fontPickerFallbackPointSize = fallbackPointSize
        fontPickerFixedOnly = fixedOnly
        fontPickerFamilies = bridge.availableFontFamilies(fixedOnly)
        fontPickerFamily = bridge.fontSettingFamily(key, fallbackFamily, fallbackPointSize)
        fontPickerPointSize = bridge.fontSettingPointSize(key, fallbackFamily, fallbackPointSize)
        fontPickerBold = bridge.fontSettingBold(key, fallbackFamily, fallbackPointSize)
        fontPickerItalic = bridge.fontSettingItalic(key, fallbackFamily, fallbackPointSize)
        fontPickerSearch = ""
        fontPicker.open()
    }

    function applyFontPicker() {
        bridge.setFontSetting(fontPickerKey,
                              fontPickerFamily,
                              fontPickerPointSize,
                              fontPickerBold,
                              fontPickerItalic,
                              fontPickerFallbackFamily,
                              fontPickerFallbackPointSize)
        refreshFontLabels()
        fontPicker.close()
    }

    function scheduleCatPersist() {
        var controller = activeCatController()
        if (controller && controller.saveSettings)
            controller.saveSettings()
        catPersistTimer.restart()
    }

    function persistSettingsNow() {
        var controller = activeCatController()
        if (controller && controller.saveSettings)
            controller.saveSettings()
        bridge.saveSettings()
    }

    function closeAfterPersist() {
        closeAlreadyPersisted = true
        persistSettingsNow()
        close()
    }

    function scheduleSettingsPersist() {
        if (!settingsDialog.visible || settingsDialog.warmupInProgress)
            return
        settingsPersistTimer.restart()
    }

    function clampToParent() {
        if (!parent) return
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.min(x, parentWidth - width))
        y = Math.max(0, Math.min(y, parentHeight - height))
    }

    function ensureInitialPosition() {
        if (positionInitialized || !parent) return
        var parentWidth = parent.width > 0 ? parent.width : width
        var parentHeight = parent.height > 0 ? parent.height : height
        x = Math.max(0, Math.round((parentWidth - width) / 2))
        y = Math.max(0, Math.round((parentHeight - height) / 2))
        positionInitialized = true
    }

    onAboutToShow: {
        if (!warmupInProgress) {
            ensureInitialPosition()
            clampToParent()
        }
    }

    onWidthChanged: {
        if (visible && !warmupInProgress)
            clampToParent()
    }

    onHeightChanged: {
        if (visible && !warmupInProgress)
            clampToParent()
    }

    onCurrentTabChanged: {
        if (!warmupInProgress)
            bridge.setSetting("uiSettingsCurrentTab", currentTab)
        if (currentTab === 7)
            refreshFrequencySettings()
    }

    onOpened: {
        if (!warmupInProgress) {
            closeAlreadyPersisted = false
            refreshCatProfileDraft()
        }
    }

    onClosed: {
        settingsPersistTimer.stop()
        catPersistTimer.stop()
        if (!warmupInProgress && !closeAlreadyPersisted)
            persistSettingsNow()
        closeAlreadyPersisted = false
    }

    Timer {
        id: warmupCloseTimer
        interval: 1
        repeat: false
        onTriggered: {
            settingsDialog.closeAlreadyPersisted = true
            settingsDialog.close()
            settingsDialog.warmupInProgress = false
            settingsDialog.positionInitialized = false
        }
    }

    Timer {
        id: catPersistTimer
        interval: 300
        repeat: false
        onTriggered: bridge.saveSettings()
    }

    Timer {
        id: settingsPersistTimer
        interval: 500
        repeat: false
        onTriggered: settingsDialog.persistSettingsNow()
    }

    // ── Theme colors ─────────────────────────────────────────────────────
    property color bgDeep:        bridge.themeManager.bgDeep
    property color bgMedium:      bridge.themeManager.bgMedium
    property color bgLight:       bridge.themeManager.bgLight
    property color bgDark:        bridge.themeManager.bgDeep
    property color primaryBlue:   bridge.themeManager.primaryColor
    property color secondaryCyan: bridge.themeManager.secondaryColor
    property color accentGreen:   bridge.themeManager.accentColor
    property color textPrimary:   bridge.themeManager.textPrimary
    property color textSecondary: bridge.themeManager.textSecondary
    property color textDim:       Qt.rgba(textSecondary.r, textSecondary.g, textSecondary.b, 0.55)
    property color glassBorder:   bridge.themeManager.glassBorder
    readonly property int controlHeight: Qt.platform.os === "linux" ? 36 : 32
    readonly property int controlFontSize: 12
    readonly property int controlVerticalPadding: Qt.platform.os === "linux" ? 1 : 0
    readonly property int spinTextSidePadding: 52

    // ── Preset colors for color pickers ──────────────────────────────────
    readonly property var presetColors: [
        "#ff0000","#ff6600","#ffcc00","#33cc33","#00ccff","#0066ff",
        "#9933ff","#ff33cc","#ffffff","#cccccc","#666666","#000000"
    ]
    readonly property var decodeColorModel: [
        { label: qsTr("Transmitted Message"),    prop: "colorTxMessage",        defaultColor: "#FFFF00" },
        { label: qsTr("My Callsign"),            prop: "colorMyCall",           defaultColor: "#FF5555" },
        { label: qsTr("New DXCC on Band"),       prop: "colorNewDxccBand",      defaultColor: "#F8AAD0" },
        { label: qsTr("New DXCC"),               prop: "colorNewDxcc",          defaultColor: "#FF00FF" },
        { label: qsTr("New Continent on Band"),  prop: "colorNewContinentBand", defaultColor: "#F5B7C7" },
        { label: qsTr("New Continent"),          prop: "colorNewContinent",     defaultColor: "#E91E63" },
        { label: qsTr("New CQ Zone on Band"),    prop: "colorNewCqZoneBand",    defaultColor: "#F5DDA0" },
        { label: qsTr("New CQ Zone"),            prop: "colorNewCqZone",        defaultColor: "#F0A030" },
        { label: qsTr("New ITU Zone on Band"),   prop: "colorNewItuZoneBand",   defaultColor: "#D4E89F" },
        { label: qsTr("New ITU Zone"),           prop: "colorNewItuZone",       defaultColor: "#9ACD32" },
        { label: qsTr("New Grid on Band"),       prop: "colorNewGridBand",      defaultColor: "#FFCAA0" },
        { label: qsTr("New Grid"),               prop: "colorNewGrid",          defaultColor: "#FF8C00" },
        { label: qsTr("New Callsign on Band"),   prop: "colorNewCallBand",      defaultColor: "#B5E8E8" },
        { label: qsTr("New Callsign"),           prop: "colorNewCall",          defaultColor: "#00E0E0" },
        { label: qsTr("LoTW marker"),            prop: "colorLotwUser",         defaultColor: "#FFFFFF" },
        { label: qsTr("CQ in Message"),          prop: "colorCQ",               defaultColor: "#33FF33" },
        { label: qsTr("DX Entity"),              prop: "colorDXEntity",         defaultColor: "#FFAA33" },
        { label: qsTr("73 / RR73"),              prop: "color73",               defaultColor: "#5599FF" },
        { label: qsTr("B4 (Worked)"),            prop: "colorB4",               defaultColor: "#888888" },
        { label: qsTr("Normal decodes"),         prop: "colorDecodeText",       defaultColor: "#AFC4D8" }
    ]

    Popup {
        id: fontPicker
        modal: true
        focus: true
        width: Math.min(settingsDialog.width - 80, 760)
        height: Math.min(settingsDialog.height - 80, 680)
        x: Math.round((settingsDialog.width - width) / 2)
        y: Math.round((settingsDialog.height - height) / 2)
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: fontSearchField.forceActiveFocus()

        background: Rectangle {
            color: bgDeep
            border.color: secondaryCyan
            border.width: 1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                text: fontPickerKey === "DecodedTextFont" ? qsTr("Choose Decoded Font") : qsTr("Choose Font")
                color: secondaryCyan
                font.pixelSize: 14
                font.bold: true
                Layout.fillWidth: true
            }

            Text {
                text: qsTr("Search:")
                color: textSecondary
                font.pixelSize: 11
            }

            DecoTextField {
                id: fontSearchField
                Layout.fillWidth: true
                implicitHeight: controlHeight
                text: settingsDialog.fontPickerSearch
                placeholderText: qsTr("filter by name")
                color: textPrimary
                font.pixelSize: controlFontSize
                topPadding: controlVerticalPadding
                bottomPadding: controlVerticalPadding
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                onTextChanged: settingsDialog.fontPickerSearch = text
                background: Rectangle {
                    color: bgMedium
                    border.color: parent.activeFocus ? secondaryCyan : glassBorder
                    radius: 4
                }
            }

            Text {
                text: settingsDialog.fontPickerFixedOnly ? qsTr("Monospaced fonts:") : qsTr("Fonts:")
                color: textSecondary
                font.pixelSize: 11
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 190
                color: bgMedium
                border.color: glassBorder
                radius: 4

                ListView {
                    id: fontFamilyList
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: settingsDialog.filteredFontFamilies()
                    currentIndex: -1
                    delegate: ItemDelegate {
                        width: fontFamilyList.width
                        height: 32
                        highlighted: modelData === settingsDialog.fontPickerFamily
                        onClicked: settingsDialog.fontPickerFamily = String(modelData)
                        background: Rectangle {
                            color: parent.highlighted
                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.32)
                                   : (parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        }
                        contentItem: Text {
                            text: modelData
                            color: textPrimary
                            font.family: modelData
                            font.pixelSize: 12
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 8

                Text { text: qsTr("Selected:"); color: textSecondary; font.pixelSize: 11 }
                Text {
                    text: settingsDialog.fontPickerFamily
                    color: textPrimary
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text { text: qsTr("Size:"); color: textSecondary; font.pixelSize: 11 }
                SpinBox {
                    id: fontPointSpin
                    from: 6
                    to: 48
                    value: settingsDialog.fontPickerPointSize
                    editable: true
                    Layout.preferredWidth: 140
                    onValueChanged: settingsDialog.fontPickerPointSize = value
                    contentItem: TextInput {
                        text: fontPointSpin.textFromValue(fontPointSpin.value, fontPointSpin.locale)
                        color: textPrimary
                        font.pixelSize: controlFontSize
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        readOnly: !fontPointSpin.editable
                        validator: fontPointSpin.validator
                    }
                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                }

                CheckBox {
                    text: qsTr("Bold")
                    checked: settingsDialog.fontPickerBold
                    onCheckedChanged: settingsDialog.fontPickerBold = checked
                    contentItem: Text { text: parent.text; leftPadding: 26; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                }

                CheckBox {
                    text: qsTr("Italic")
                    checked: settingsDialog.fontPickerItalic
                    onCheckedChanged: settingsDialog.fontPickerItalic = checked
                    contentItem: Text { text: parent.text; leftPadding: 26; color: textPrimary; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                }

                Item { Layout.fillWidth: true }
                Item { Layout.preferredWidth: 140 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: glassBorder
                radius: 4
                Text {
                    anchors.fill: parent
                    anchors.margins: 10
                    text: "173045  -21  0.1  1045  CQ LB9ZG JP20"
                    color: textPrimary
                    font.family: settingsDialog.fontPickerFamily
                    font.pointSize: settingsDialog.fontPickerPointSize
                    font.bold: settingsDialog.fontPickerBold
                    font.italic: settingsDialog.fontPickerItalic
                    wrapMode: Text.Wrap
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    onClicked: fontPicker.close()
                }
                Button {
                    text: qsTr("Apply")
                    Layout.fillWidth: true
                    enabled: settingsDialog.fontPickerFamily !== ""
                    onClicked: settingsDialog.applyFontPicker()
                }
            }
        }
    }

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: secondaryCyan; border.width: 2; radius: 12

        // Keep this dialog compatible with the Linux Qt 6.4 AppImage runtime.
        // QtQuick.Effects/MultiEffect is only available from Qt 6.5.
        layer.enabled: false
    }

    // 1.0.180 — Apertura/chiusura su render thread con OpacityAnimator.
    enter: Transition {
        OpacityAnimator { from: 0.0; to: 1.0; duration: 180; easing.type: Easing.OutQuad }
    }
    exit: Transition {
        OpacityAnimator { from: 1.0; to: 0.0; duration: 120; easing.type: Easing.InQuad }
    }

    // ── Draggable header ─────────────────────────────────────────────────
    header: Rectangle {
        height: 56
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)

        MouseArea {
            anchors.fill: parent
            property point clickPos: Qt.point(0, 0)
            cursorShape: Qt.SizeAllCursor
            onPressed: function(mouse) {
                clickPos = Qt.point(mouse.x, mouse.y)
                settingsDialog.positionInitialized = true
            }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                settingsDialog.x += mouse.x - clickPos.x
                settingsDialog.y += mouse.y - clickPos.y
                settingsDialog.clampToParent()
            }
        }

        RowLayout {
            anchors.fill: parent; anchors.margins: 16; spacing: 10

            Text {
                text: qsTr("Settings")
                font.pixelSize: 16; font.bold: true
                color: secondaryCyan
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                width: 34; height: 34; radius: 6
                color: closeMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.3) : Qt.rgba(1,1,1,0.1)
                border.color: closeMA.containsMouse ? "#f44336" : glassBorder
                Text { anchors.centerIn: parent; text: "\u2715"; color: closeMA.containsMouse ? "#f44336" : textPrimary; font.pixelSize: 14 }
                MouseArea { id: closeMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.close() }
            }
        }
    }

    footer: Rectangle {
        height: 64
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.96)
        border.color: glassBorder
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: settingsDialog.width < 520 ? 8 : 12
            spacing: settingsDialog.width < 520 ? 8 : 10

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: qsTr("Changes are applied immediately where supported.")
                color: textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.preferredWidth: settingsDialog.width < 520 ? 84 : 110
                Layout.minimumWidth: 72
                Layout.preferredHeight: 36
                radius: 6
                color: closeFooterMA.containsMouse ? Qt.rgba(1,1,1,0.08) : bgMedium
                border.color: glassBorder

                Text {
                    anchors.centerIn: parent
                    text: qsTr("Close")
                    color: textPrimary
                    font.pixelSize: 12
                }

                MouseArea {
                    id: closeFooterMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.close()
                }
            }

            Rectangle {
                Layout.preferredWidth: settingsDialog.width < 520 ? 84 : 110
                Layout.minimumWidth: 72
                Layout.preferredHeight: 36
                radius: 6
                color: okFooterMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.22) : bgMedium
                border.color: accentGreen

                Text {
                    anchors.centerIn: parent
                    text: qsTr("OK")
                    color: accentGreen
                    font.pixelSize: 12
                    font.bold: true
                }

                MouseArea {
                    id: okFooterMA
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settingsDialog.closeAfterPersist()
                }
            }
        }
    }

    // ── Content ──────────────────────────────────────────────────────────
    contentItem: Item {
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ── Sidebar ──────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: settingsDialog.compactSettingsLayout ? 180 : 210
                Layout.minimumWidth: settingsDialog.compactSettingsLayout ? 160 : 210
                Layout.fillHeight: true
                color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)

                Column {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    spacing: 2

                    Repeater {
                        model: [qsTr("Station"), qsTr("Radio"), qsTr("Audio"), qsTr("TX"), qsTr("Display"), qsTr("Decode"), qsTr("Reporting"), qsTr("Frequencies"), qsTr("Colors"), qsTr("Advanced"), qsTr("Alerts"), qsTr("Filters"), qsTr("UI Buttons")]
                        delegate: Rectangle {
                            width: parent.width; height: 36; radius: 6
                            color: tabStack.currentIndex === index ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (tabMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent")
                            border.color: tabStack.currentIndex === index ? primaryBlue : "transparent"
                            Text { anchors.centerIn: parent; text: modelData; color: tabStack.currentIndex === index ? primaryBlue : textSecondary; font.pixelSize: 12 }
                            MouseArea { id: tabMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.currentTab = index }
                        }
                    }
                }
            }

            // Vertical separator
            Rectangle { Layout.fillHeight: true; width: 1; color: glassBorder }

            // ── StackLayout ──────────────────────────────────────────
            StackLayout {
                id: tabStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsDialog.currentTab

                // ═══════════ TAB 0 — STAZIONE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Dettagli Stazione ──
                        Text { text: qsTr("STATION DETAILS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("My Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.callsign; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.callsign = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }
                        Text { text: qsTr("My Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.grid; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.grid = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }

                        Text { text: qsTr("Auto Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        CheckBox {
                            checked: bridge.getSetting("AutoGrid", false)
                            onCheckedChanged: bridge.setSetting("AutoGrid", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("IARU Region:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            model: ["1","2","3"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Region", 0))
                            onActivated: bridge.setSetting("Region", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                        }

                        Text { text: qsTr("Type 2 Msg Gen:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            model: [qsTr("Full"),qsTr("Type 1 prefix"),qsTr("Type 2 prefix")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("Type2MsgGen", 0))
                            onActivated: bridge.setSetting("Type2MsgGen", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                        }
                        Text { text: qsTr("Op Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.getSetting("OpCall", ""); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OpCall", text)
                        }

                        // ── Info Stazione ──
                        Text { text: qsTr("STATION INFO"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Station Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.stationName; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.stationName = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }
                        Text { text: qsTr("QTH:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.stationQth; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.stationQth = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }

                        Text { text: qsTr("Rig Info:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.stationRigInfo; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.stationRigInfo = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }
                        Text { text: qsTr("Antenna:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            text: bridge.stationAntenna; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.stationAntenna = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }

                        Text { text: qsTr("Power (W):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        SpinBox {
                            id: stPowerSpin
                            from: 0; to: 9999; value: bridge.stationPowerWatts; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth
                            onValueChanged: {
                                bridge.stationPowerWatts = value
                                settingsDialog.scheduleSettingsPersist()
                            }
                            contentItem: TextInput { text: stPowerSpin.textFromValue(stPowerSpin.value, stPowerSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !stPowerSpin.editable; validator: stPowerSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("WSPR Power:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: wsprPowerCombo
                            model: bridge.wsprPowerOptions()
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            function optionIndexFor(dbm) {
                                var needle = String(Number(dbm || 37)) + " dBm"
                                for (var i = 0; i < model.length; ++i) {
                                    if (String(model[i]).indexOf(needle) === 0)
                                        return i
                                }
                                return 11
                            }
                            currentIndex: optionIndexFor(bridge.wsprPowerDbm)
                            onActivated: function(index) {
                                if (index < 0 || index >= model.length)
                                    return
                                var dbm = parseInt(String(model[index] || ""), 10)
                                if (!isNaN(dbm)) {
                                    bridge.wsprPowerDbm = dbm
                                    settingsDialog.scheduleSettingsPersist()
                                }
                            }
                            Connections {
                                target: bridge
                                function onWsprPowerDbmChanged() {
                                    wsprPowerCombo.currentIndex = wsprPowerCombo.optionIndexFor(bridge.wsprPowerDbm)
                                }
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                        }
                    }
                }

                // ═══════════ TAB 1 — RADIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Backend CAT ──
                        Text { text: qsTr("BACKEND CAT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Backend:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Row {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 6
                            Repeater {
                                model: [["native",qsTr("Native (15 radios)")],["hamlib",qsTr("Hamlib (300+ radios)")],["tci","TCI"],["omnirig","OmniRig"]]
                                delegate: Rectangle {
                                    property string bk: modelData[0]
                                    property bool active: bridge.catBackend === bk
                                    property bool catBusy: settingsDialog.catConnectionInProgress()
                                    width: 170; height: 30; radius: 6
                                    color: active ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.25) : (catBusy ? Qt.rgba(1,1,1,0.025) : (bkMA.containsMouse ? Qt.rgba(1,1,1,0.05) : "transparent"))
                                    border.color: active ? primaryBlue : glassBorder
                                    Text { anchors.centerIn: parent; text: modelData[1]; color: active ? primaryBlue : (catBusy ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.55) : textSecondary); font.pixelSize: 11 }
                                    MouseArea { id: bkMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: {
                                            bridge.catBackend = bk
                                            if (bk === "tci")
                                                settingsDialog.selectTciRigIfNeeded()
                                            settingsDialog.scheduleCatPersist()
                                        }
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("Profile:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 6

                            DecoComboBox {
                                id: catProfileCombo
                                model: bridge.catProfileList || []
                                Layout.preferredWidth: compactSettingsLayout ? 180 : 240
                                Layout.minimumWidth: compactSettingsLayout ? 150 : 200
                                implicitHeight: controlHeight
                                currentIndex: {
                                    var active = String(bridge.activeCatProfile || "")
                                    if (active.length === 0)
                                        return -1
                                    return find(active)
                                }
                                onActivated: {
                                    catProfileNameField.text = currentText
                                    settingsDialog.loadSelectedCatProfile()
                                }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text {
                                    text: catProfileCombo.currentIndex >= 0 ? catProfileCombo.displayText : qsTr("No profile")
                                    color: catProfileCombo.currentIndex >= 0 ? textPrimary : textSecondary
                                    font.pixelSize: controlFontSize
                                    leftPadding: 8
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                delegate: ItemDelegate {
                                    contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                    background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
                                }
                                popup: SettingsComboPopup { combo: catProfileCombo }
                            }

                            DecoTextField {
                                id: catProfileNameField
                                Layout.fillWidth: true
                                Layout.minimumWidth: compactSettingsLayout ? 180 : 260
                                implicitHeight: controlHeight
                                text: bridge.activeCatProfile || (bridge.suggestedCatProfileName ? bridge.suggestedCatProfileName() : "")
                                placeholderText: qsTr("Profile name")
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                leftPadding: 8
                                onAccepted: settingsDialog.saveCatProfileFromField()
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            }

                            Button {
                                id: catProfileLoadButton
                                text: qsTr("Load")
                                enabled: catProfileCombo.currentIndex >= 0 && !settingsDialog.catConnectionInProgress()
                                Layout.preferredWidth: 68
                                implicitHeight: controlHeight
                                onClicked: settingsDialog.loadSelectedCatProfile()
                                background: Rectangle { color: catProfileLoadButton.enabled && catProfileLoadButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.22) : bgMedium; border.color: catProfileLoadButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                                contentItem: Text { text: catProfileLoadButton.text; color: catProfileLoadButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }

                            Button {
                                id: catProfileSaveButton
                                text: qsTr("Save")
                                enabled: String(catProfileNameField.text || "").trim().length > 0
                                Layout.preferredWidth: 68
                                implicitHeight: controlHeight
                                onClicked: settingsDialog.saveCatProfileFromField()
                                background: Rectangle { color: catProfileSaveButton.enabled && catProfileSaveButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.22) : bgMedium; border.color: catProfileSaveButton.enabled ? accentGreen : glassBorder; radius: 4 }
                                contentItem: Text { text: catProfileSaveButton.text; color: catProfileSaveButton.enabled ? accentGreen : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }

                            Button {
                                id: catProfileNewButton
                                text: qsTr("New")
                                Layout.preferredWidth: 62
                                implicitHeight: controlHeight
                                onClicked: settingsDialog.saveNewCatProfileFromCurrent()
                                background: Rectangle { color: catProfileNewButton.hovered ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.18) : bgMedium; border.color: secondaryCyan; radius: 4 }
                                contentItem: Text { text: catProfileNewButton.text; color: secondaryCyan; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }

                            Button {
                                id: catProfileDeleteButton
                                text: qsTr("Delete")
                                enabled: catProfileCombo.currentIndex >= 0
                                Layout.preferredWidth: 76
                                implicitHeight: controlHeight
                                onClicked: settingsDialog.deleteSelectedCatProfile()
                                background: Rectangle { color: catProfileDeleteButton.enabled && catProfileDeleteButton.hovered ? Qt.rgba(1,0.25,0.25,0.18) : bgMedium; border.color: catProfileDeleteButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                                contentItem: Text { text: catProfileDeleteButton.text; color: catProfileDeleteButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }

                        // Banner: porta seriale occupata da altro software
                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            visible: bridge.lastCatError.indexOf("occupata") !== -1
                            implicitHeight: visible ? (settingsBannerText.implicitHeight + 16) : 0
                            Rectangle {
                                anchors.fill: parent
                                color: Qt.rgba(1.0, 0.65, 0.0, 0.15)
                                border.color: Qt.rgba(1.0, 0.65, 0.0, 0.6)
                                border.width: 1
                                radius: 6
                                Text {
                                    id: settingsBannerText
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    wrapMode: Text.WordWrap
                                    color: textPrimary
                                    font.pixelSize: 11
                                    text: bridge.lastCatError + "\n" + qsTr("Tip: close OmniRig from the Windows tray icon, then press Connect again.")
                                }
                            }
                        }

                        // ── Stato connessione ──
                        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Row {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 8
                            Rectangle { width: 12; height: 12; radius: 6; color: bridge.catConnected ? accentGreen : "#f44336"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: bridge.catConnected ? qsTr("Connected") + " — " + bridge.catRigName + " — " + bridge.catMode : qsTr("Disconnected"); color: bridge.catConnected ? accentGreen : "#f44336"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Item { width: 20; height: 1 }
                            Rectangle {
                                width: 100; height: 28; radius: 6
                                color: connMA.containsMouse ? (bridge.catConnected ? Qt.rgba(0.95,0.26,0.21,0.2) : Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.2)) : "transparent"
                                border.color: bridge.catConnected ? "#f44336" : accentGreen
                                Text { anchors.centerIn: parent; text: bridge.catConnected ? qsTr("Disconnect") : qsTr("Connect"); color: bridge.catConnected ? "#f44336" : accentGreen; font.pixelSize: 11 }
                                MouseArea { id: connMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsDialog.toggleCatConnection()
                                }
                            }
                            Rectangle {
                                width: 28; height: 28; radius: 6
                                color: refreshMA.containsMouse ? bgMedium : "transparent"
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: "↻"; color: secondaryCyan; font.pixelSize: 16 }
                                MouseArea { id: refreshMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsDialog.refreshCatPorts()
                                }
                            }
                        }

                        // ── Controllo CAT ──
                        Text { text: qsTr("CAT CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Rig:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: rigCombo
                            model: bridge.catBackend === "tci" ? ["TCI Client RX1", "TCI Client RX2"] : (bridge.catManager ? bridge.catManager.rigList : []); Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            property string filterText: ""
                            property var filteredRigList: {
                                var src = bridge.catBackend === "tci" ? ["TCI Client RX1", "TCI Client RX2"] : (bridge.catManager ? bridge.catManager.rigList : [])
                                var q = filterText.trim().toLowerCase()
                                if (q.length === 0)
                                    return src

                                var terms = q.split(/\s+/)
                                var out = []
                                for (var i = 0; i < src.length; ++i) {
                                    var name = String(src[i])
                                    var haystack = name.toLowerCase()
                                    var match = true
                                    for (var t = 0; t < terms.length; ++t) {
                                        if (terms[t].length > 0 && haystack.indexOf(terms[t]) < 0) {
                                            match = false
                                            break
                                        }
                                    }
                                    if (match)
                                        out.push(name)
                                }
                                return out
                            }
                            function chooseRig(name) {
                                var idx = model.indexOf(name)
                                if (idx >= 0)
                                    currentIndex = idx
                                if (bridge.catManager) {
                                    bridge.catManager.rigName = name
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                                rigComboPopup.close()
                            }
                            currentIndex: {
                                if (!bridge.catManager)
                                    return -1
                                return find(bridge.catManager.rigName)
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.rigName = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text {
                                text: rigCombo.currentIndex >= 0 ? rigCombo.displayText : settingsDialog.activeRigName()
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            popup: Popup {
                                id: rigComboPopup
                                parent: Overlay.overlay
                                readonly property var comboOrigin: rigCombo && parent ? rigCombo.mapToItem(parent, 0, 0) : Qt.point(0, 0)
                                readonly property real wantedHeight: Math.min(420,
                                                 Math.max(180,
                                                          Math.min(settingsDialog.height - 160,
                                                                   54 + Math.max(34, rigComboPopupList.contentHeight))))
                                readonly property real spaceBelow: parent ? parent.height - comboOrigin.y - rigCombo.height - 8 : wantedHeight
                                readonly property real spaceAbove: parent ? comboOrigin.y - 8 : 0
                                readonly property bool openAbove: wantedHeight > spaceBelow && spaceAbove > spaceBelow
                                x: parent ? Math.max(8, Math.min(comboOrigin.x, parent.width - width - 8)) : 0
                                y: parent
                                   ? (openAbove
                                      ? Math.max(8, comboOrigin.y - height - 2)
                                      : Math.min(comboOrigin.y + rigCombo.height + 2, parent.height - height - 8))
                                   : 0
                                width: parent ? Math.min(Math.max(rigCombo.width, 560), Math.max(80, parent.width - 16))
                                              : Math.max(rigCombo.width, 560)
                                height: Math.min(420,
                                                 Math.max(180,
                                                          Math.min(settingsDialog.height - 160,
                                                                   54 + Math.max(34, rigComboPopupList.contentHeight))))
                                focus: true
                                onOpened: {
                                    rigCombo.filterText = ""
                                    rigSearchField.forceActiveFocus()
                                }
                                background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                                contentItem: Column {
                                    width: rigComboPopup.width
                                    spacing: 6

                                    DecoTextField {
                                        id: rigSearchField
                                        x: 8
                                        width: parent.width - 16
                                        height: 36
                                        placeholderText: qsTr("Search radio, model or brand...")
                                        text: rigCombo.filterText
                                        selectByMouse: true
                                        color: textPrimary
                                        placeholderTextColor: textSecondary
                                        font.pixelSize: controlFontSize
                                        leftPadding: 10
                                        rightPadding: 10
                                        onTextChanged: rigCombo.filterText = text
                                        background: Rectangle {
                                            color: bgMedium
                                            border.color: activeFocus ? secondaryCyan : glassBorder
                                            radius: 4
                                        }
                                    }

                                    ListView {
                                        id: rigComboPopupList
                                        x: 8
                                        width: parent.width - 16
                                        height: rigComboPopup.height - rigSearchField.height - 22
                                        clip: true
                                        model: rigCombo.filteredRigList
                                        currentIndex: -1
                                        boundsBehavior: Flickable.StopAtBounds
                                        flickableDirection: Flickable.VerticalFlick
                                        interactive: true
                                        focus: true
                                        reuseItems: true
                                        delegate: ItemDelegate {
                                            width: rigComboPopupList.width
                                            height: 34
                                            highlighted: modelData === settingsDialog.activeRigName()
                                            contentItem: Text {
                                                text: modelData
                                                color: parent.highlighted ? secondaryCyan : textPrimary
                                                font.pixelSize: 12
                                                verticalAlignment: Text.AlignVCenter
                                                elide: Text.ElideRight
                                            }
                                            background: Rectangle {
                                                color: hovered || parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                            }
                                            onClicked: rigCombo.chooseRig(modelData)
                                        }
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("Serial Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        RowLayout {
                            visible: settingsDialog.usesSerialControls()
                            Layout.fillWidth: true
                            Layout.minimumWidth: wideFieldMinWidth
                            spacing: 8

                            DecoComboBox {
                                id: serialPortCombo
                                visible: settingsDialog.usesSerialControls()
                                model: bridge.catManager ? bridge.catManager.portList : []
                                Layout.fillWidth: true
                                implicitHeight: controlHeight
                                currentIndex: {
                                    if (!bridge.catManager)
                                        return -1
                                    return find(bridge.catManager.serialPort)
                                }
                                onActivated: {
                                    if (bridge.catManager) {
                                        bridge.catManager.serialPort = currentText
                                        settingsDialog.enforceForceLineAvailability()
                                    }
                                    settingsDialog.scheduleCatPersist()
                                }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text {
                                    text: serialPortCombo.currentIndex >= 0 ? serialPortCombo.displayText : (bridge.catManager ? bridge.catManager.serialPort : "")
                                    color: textPrimary
                                    font.pixelSize: controlFontSize
                                    leftPadding: 8
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                                delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                    background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                                popup: SettingsComboPopup { combo: serialPortCombo }
                            }

                            Rectangle {
                                id: serialPortRefreshButton
                                Layout.preferredWidth: controlHeight
                                Layout.preferredHeight: controlHeight
                                radius: 4
                                color: serialPortRefreshMA.containsMouse ? bgMedium : "transparent"
                                border.color: secondaryCyan
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "↻"
                                    color: secondaryCyan
                                    font.pixelSize: 17
                                    font.bold: true
                                }

                                MouseArea {
                                    id: serialPortRefreshMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: settingsDialog.refreshCatPorts()
                                }

                                ToolTip.visible: serialPortRefreshMA.containsMouse
                                ToolTip.text: qsTr("Refresh serial ports")
                            }
                        }
                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("Baud Rate:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        DecoComboBox {
                            id: baudCombo
                            visible: settingsDialog.usesSerialControls()
                            model: bridge.catManager && bridge.catManager.baudList ? bridge.catManager.baudList : ["4800","9600","19200","38400","57600","115200"]
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                var baud = settingsDialog.activeBaudRateText()
                                return baud === "" ? -1 : settingsDialog.stringListIndexOf(model, baud)
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.baudRate = parseInt(currentText)
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text {
                                text: baudCombo.currentIndex >= 0 ? baudCombo.displayText : settingsDialog.activeBaudRateText()
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                            }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: baudCombo }
                        }

                        // ── CI-V Address (solo rig ICOM) ──
                        Text {
                            visible: settingsDialog.usesSerialControls() && settingsDialog.rigIsIcom()
                            text: qsTr("CI-V Addr:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        Rectangle {
                            id: civAddrField
                            visible: settingsDialog.usesSerialControls() && settingsDialog.rigIsIcom()
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            color: bgMedium
                            border.color: glassBorder
                            radius: 4
                            clip: true

                            readonly property string valueText: settingsDialog.civAddressText()

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                text: civAddrField.valueText.length > 0
                                      ? civAddrField.valueText
                                      : settingsDialog.civAddressPlaceholderText()
                                color: civAddrField.valueText.length > 0 ? textPrimary : textSecondary
                                font.pixelSize: controlFontSize
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Text {
                            visible: settingsDialog.usesNetworkControls()
                            text: qsTr("Host:Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        DecoTextField {
                            visible: settingsDialog.usesNetworkControls()
                            text: bridge.catManager ? bridge.catManager.networkPort : ""
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe" ? "127.0.0.1:7809" : "host:port"
                            selectByMouse: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: {
                                if (bridge.catManager) bridge.catManager.networkPort = text.trim()
                                settingsDialog.scheduleCatPersist()
                            }
                        }

                        Text {
                            visible: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe"
                            text: qsTr("HRD Radio:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        CheckBox {
                            id: hrdStrictRadioMatchCheck
                            visible: bridge.catManager && bridge.catManager.rigName === "Ham Radio Deluxe"
                            checked: bridge.catManager ? bridge.catManager.hrdStrictRadioMatch : true
                            text: qsTr("Strict match (abort if configured radio is not current in HRD)")
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            onCheckedChanged: {
                                if (bridge.catManager && bridge.catManager.hrdStrictRadioMatch !== checked) {
                                    bridge.catManager.hrdStrictRadioMatch = checked
                                    settingsDialog.scheduleCatPersist()
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: 26
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Text {
                            visible: settingsDialog.usesTciControls()
                            text: qsTr("TCI Host:Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        DecoTextField {
                            visible: settingsDialog.usesTciControls()
                            text: bridge.catManager ? bridge.catManager.tciPort : ""
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "localhost:50001"
                            selectByMouse: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                if (bridge.catManager) bridge.catManager.tciPort = text
                                settingsDialog.scheduleCatPersist()
                            }
                        }

                        Text {
                            visible: settingsDialog.usesTciControls()
                            text: qsTr("TCI Audio:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        CheckBox {
                            visible: settingsDialog.usesTciControls()
                            checked: bridge.catManager ? bridge.catManager.tciAudioEnabled : true
                            text: qsTr("RX/TX via TCI")
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            onCheckedChanged: {
                                if (bridge.catManager) bridge.catManager.tciAudioEnabled = checked
                                settingsDialog.scheduleCatPersist()
                            }
                            contentItem: Text {
                                text: parent.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: 26
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Text { text: qsTr("PTT Method:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: pttCombo
                            enabled: !settingsDialog.usesTciControls()
                            model: settingsDialog.usesTciControls()
                                   ? ["CAT"]
                                   : (bridge.catManager && bridge.catManager.pttMethodList ? bridge.catManager.pttMethodList : ["CAT","DTR","RTS","VOX"])
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (settingsDialog.usesTciControls())
                                    return 0
                                var methods = (bridge.catManager && bridge.catManager.pttMethodList)
                                              ? bridge.catManager.pttMethodList
                                              : ["CAT","DTR","RTS","VOX"]
                                var savedMethod = bridge.catManager ? bridge.catManager.pttMethod : "CAT"
                                var idx = settingsDialog.stringListIndexOf(methods, savedMethod)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.pttMethod = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text {
                                text: {
                                    if (pttCombo.currentIndex >= 0 && pttCombo.displayText !== "")
                                        return pttCombo.displayText
                                    if (bridge.catManager && bridge.catManager.pttMethod !== undefined && bridge.catManager.pttMethod !== null) {
                                        var fallback = String(bridge.catManager.pttMethod).trim().toUpperCase()
                                        return fallback !== "" ? fallback : "CAT"
                                    }
                                    return "CAT"
                                }
                                color: pttCombo.enabled ? textPrimary : textSecondary
                                font.pixelSize: controlFontSize
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                            }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: pttCombo }
                        }
                        Text {
                            visible: settingsDialog.usesSeparatePttPort()
                            text: qsTr("PTT Port:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: labelWidth
                        }
                        DecoComboBox {
                            id: pttPortCombo
                            visible: settingsDialog.usesSeparatePttPort()
                            model: settingsDialog.pttPortOptions()
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            currentIndex: {
                                if (!bridge.catManager)
                                    return -1
                                var idx = find(bridge.catManager.pttPort)
                                return idx >= 0 ? idx : (count > 0 ? 0 : -1)
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.pttPort = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: pttPortCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: pttPortCombo }
                        }
                        Item { visible: settingsDialog.usesSeparatePttPort(); Layout.fillWidth: true; Layout.columnSpan: 2 }
                        Text { text: qsTr("Poll Interval (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: pollSpin
                            from: 1; to: 99; value: bridge.catManager ? bridge.catManager.pollInterval : 3; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: {
                                if (bridge.catManager) bridge.catManager.pollInterval = value
                                settingsDialog.scheduleCatPersist()
                            }
                            contentItem: TextInput { text: pollSpin.textFromValue(pollSpin.value, pollSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !pollSpin.editable; validator: pollSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("CAT keep-alive:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        CheckBox {
                            visible: settingsDialog.usesSerialControls()
                            checked: bridge.catManager ? bridge.catManager.catKeepAlive : false
                            text: qsTr("Light polling for interface activity LEDs")
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            onCheckedChanged: {
                                if (bridge.catManager && bridge.catManager.catKeepAlive !== checked)
                                    bridge.catManager.catKeepAlive = checked
                                settingsDialog.scheduleCatPersist()
                            }
                            contentItem: Text {
                                text: parent.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: 26
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                        }

                        // ── Parametri Seriali ──
                        Text {
                            visible: settingsDialog.usesSerialControls()
                            text: qsTr("SERIAL PARAMETERS")
                            color: secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            Layout.columnSpan: 4
                            Layout.topMargin: 10
                        }
                        Rectangle {
                            visible: settingsDialog.usesSerialControls()
                            Layout.fillWidth: true
                            Layout.columnSpan: 4
                            height: 1
                            color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3)
                        }

                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Data Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: dataBitsCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["Default","8","7"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!bridge.catManager)
                                    return 0
                                return settingsDialog.catSerialChoiceIndex(model, bridge.catManager.dataBits, 0)
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.dataBits = currentText
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: dataBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: dataBitsCombo }
                        }
                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Stop Bits:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: stopBitsCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["Default","1","2"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                return settingsDialog.catSerialChoiceIndex(model, settingsDialog.activeStopBitsText(), 0)
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.stopBits = currentText
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: stopBitsCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: stopBitsCombo }
                        }

                        Text { visible: settingsDialog.usesSerialControls(); text: qsTr("Handshake:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: handshakeCombo
                            visible: settingsDialog.usesSerialControls()
                            model: ["Default","none","xonxoff","hardware"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!bridge.catManager)
                                    return 0
                                return settingsDialog.catSerialChoiceIndex(model, bridge.catManager.handshake, 0)
                            }
                            onActivated: {
                                if (bridge.catManager) {
                                    bridge.catManager.handshake = currentText
                                    settingsDialog.enforceForceLineAvailability()
                                }
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.handshakeChoiceLabel(handshakeCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.handshakeChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: handshakeCombo }
                        }
                        Item { visible: settingsDialog.usesSerialControls(); Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { visible: settingsDialog.usesSerialControls(); enabled: settingsDialog.forceDtrControlEnabled(); text: qsTr("Force DTR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: forceDtrCombo
                            visible: settingsDialog.usesSerialControls()
                            enabled: settingsDialog.forceDtrControlEnabled()
                            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!enabled || !bridge.catManager)
                                    return 0
                                var v = settingsDialog.forceLineMode(bridge.catManager.forceDtr, bridge.catManager.dtrHigh)
                                var idx = find(v)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: settingsDialog.applyForceLineValue("dtr", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            // Lookup diretto su model[currentIndex] — displayText non si propaga
                            // affidabilmente al primo render con model JS array (Qt 6 quirk).
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(forceDtrCombo.model[Math.max(0, forceDtrCombo.currentIndex)]); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: forceDtrCombo }
                        }
                        Text { visible: settingsDialog.usesSerialControls(); enabled: settingsDialog.forceRtsControlEnabled(); text: qsTr("Force RTS:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: forceRtsCombo
                            visible: settingsDialog.usesSerialControls()
                            enabled: settingsDialog.forceRtsControlEnabled()
                            model: ["Default","On","Off"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: {
                                if (!enabled || !bridge.catManager)
                                    return 0
                                var v = settingsDialog.forceLineMode(bridge.catManager.forceRts, bridge.catManager.rtsHigh)
                                var idx = find(v)
                                return idx >= 0 ? idx : 0
                            }
                            onActivated: settingsDialog.applyForceLineValue("rts", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(forceRtsCombo.model[Math.max(0, forceRtsCombo.currentIndex)]); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: forceRtsCombo }
                        }

                        // ── Operazione Split ──
                        Text { text: qsTr("SPLIT OPERATION"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Split:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: splitCombo
                            model: settingsDialog.splitModeOptions(); Layout.fillWidth: true; implicitHeight: controlHeight
                            textRole: "label"
                            currentIndex: {
                                if (!bridge.catManager)
                                    return 0
                                for (var i = 0; i < splitCombo.model.length; ++i) {
                                    if (splitCombo.model[i].value === String(bridge.catManager.splitMode))
                                        return i
                                }
                                return 0
                            }
                            onActivated: {
                                if (bridge.catManager) bridge.catManager.splitMode = splitCombo.model[currentIndex].value
                                settingsDialog.scheduleCatPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: splitCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData.label; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: splitCombo }
                        }
                        Text { text: qsTr("Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: modeCombo
                            model: ["USB","Data/Pkt","None"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: settingsDialog.settingChoiceIndex("CATMode", model, 0)
                            onActivated: bridge.setSetting("CATMode", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(modeCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: modeCombo }
                        }

                        Text { visible: !settingsDialog.usesTciControls(); text: qsTr("TX Audio Src:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: txAudioSrcCombo
                            visible: !settingsDialog.usesTciControls()
                            model: ["Rear/Data","Front/Mic"]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: settingsDialog.settingChoiceIndex("TXAudioSource", model, 0)
                            onActivated: bridge.setSetting("TXAudioSource", currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: settingsDialog.setupChoiceLabel(txAudioSrcCombo.displayText); color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: settingsDialog.setupChoiceLabel(modelData); color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: txAudioSrcCombo }
                        }
                        Text { visible: settingsDialog.usesTciControls(); text: qsTr("TX Audio:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            visible: settingsDialog.usesTciControls()
                            text: qsTr("TCI Audio")
                            readOnly: true
                            enabled: false
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textSecondary
                            font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Diagnostica ──
                        Text { text: qsTr("DIAGNOSTICS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Check SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.supportsSwrTelemetry() ? bridge.getSetting("CheckSWR", false) : false
                            enabled: settingsDialog.supportsSwrTelemetry()
                            onCheckedChanged: if (enabled) {
                                bridge.setSetting("CheckSWR", checked)
                                if (checked && !bridge.getSetting("PWRandSWR", false))
                                    bridge.setSetting("PWRandSWR", true)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("PWR and SWR:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.supportsSwrTelemetry() ? bridge.getSetting("PWRandSWR", false) : false
                            enabled: settingsDialog.supportsSwrTelemetry()
                            onCheckedChanged: if (enabled) bridge.setSetting("PWRandSWR", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // Soglia SWR oltre la quale il TX viene bloccato/interrotto (protezione PA).
                        // Configurabile: utile per il CW e per antenne con SWR moderato. Default 2.5.
                        Text { text: qsTr("SWR max:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            enabled: settingsDialog.supportsSwrTelemetry()
                            model: ["2.0","2.5","3.0","3.5","4.0"]
                            Layout.fillWidth: true; Layout.columnSpan: 3; implicitHeight: controlHeight
                            currentIndex: Math.max(0, model.indexOf(Number(bridge.getSetting("SWRStopThreshold", 2.5)).toFixed(1)))
                            onActivated: bridge.setSetting("SWRStopThreshold", Number(currentText))
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                        }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                property bool catBusy: settingsDialog.catConnectionInProgress()
                                width: 100; height: controlHeight; radius: 4
                                color: catBusy ? bgMedium : (catConnMA.containsMouse ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.3) : bgMedium)
                                border.color: catBusy ? glassBorder : accentGreen
                                Text { anchors.centerIn: parent; text: parent.catBusy ? qsTr("Connecting...") : qsTr("Connect"); color: parent.catBusy ? textSecondary : accentGreen; font.pixelSize: 12 }
                                MouseArea { id: catConnMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { var controller = settingsDialog.activeCatController(); if (controller) controller.connectRig() } }
                            }
                            Rectangle {
                                property bool catBusy: settingsDialog.catConnectionInProgress()
                                width: 100; height: controlHeight; radius: 4
                                color: catBusy ? bgMedium : (catDiscMA.containsMouse ? Qt.rgba(1,0.3,0.3,0.3) : bgMedium)
                                border.color: catBusy ? glassBorder : "#f44336"
                                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: parent.catBusy ? textSecondary : "#f44336"; font.pixelSize: 12 }
                                MouseArea { id: catDiscMA; anchors.fill: parent; hoverEnabled: true; enabled: !parent.catBusy; cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: { var controller = settingsDialog.activeCatController(); if (controller) controller.disconnectRig() } }
                            }
                        }
                        Text {
                            visible: bridge.catBackend === "hamlib"
                            text: qsTr("Hamlib:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: labelWidth
                        }
                        RowLayout {
                            visible: bridge.catBackend === "hamlib"
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 10
                            Rectangle {
                                width: 180; height: controlHeight; radius: 4
                                color: hamlibUpdateMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Open Hamlib update"); color: primaryBlue; font.pixelSize: 12 }
                                MouseArea {
                                    id: hamlibUpdateMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.openHamlibUpdatePage()
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Windows: DLL updated from the Hamlib site. macOS/Linux: official documentation and releases.")
                                wrapMode: Text.Wrap
                                color: textSecondary
                                font.pixelSize: 11
                            }
                        }

                        // ── ALC AUTO CALIBRATION (1.0.324) ──
                        Text {
                            text: qsTr("ALC AUTO CALIBRATION")
                            color: secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            Layout.columnSpan: 4
                            Layout.topMargin: 10
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.columnSpan: 4
                            height: 1
                            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.3)
                        }

                        Text {
                            text: qsTr("ALC target:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                            ToolTip.visible: alcTargetHover.containsMouse
                            ToolTip.delay: 600
                            ToolTip.text: qsTr("ALC scale 0-100. FT8/data: typically 15-25. Values >60 risk overdriving the PA.")
                            HoverHandler { id: alcTargetHover }
                        }
                        SpinBox {
                            id: alcTargetSpinBox
                            from: 5
                            to: 60
                            value: bridge.alcTarget
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            onValueModified: bridge.setAlcTarget(value)
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            contentItem: Text {
                                text: alcTargetSpinBox.value
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            up.indicator: Rectangle {
                                x: alcTargetSpinBox.mirrored ? 0 : parent.width - width
                                width: 28; height: parent.height
                                color: "transparent"
                                Text { anchors.centerIn: parent; text: "+"; color: textPrimary; font.pixelSize: 14 }
                            }
                            down.indicator: Rectangle {
                                x: alcTargetSpinBox.mirrored ? parent.width - width : 0
                                width: 28; height: parent.height
                                color: "transparent"
                                Text { anchors.centerIn: parent; text: "-"; color: textPrimary; font.pixelSize: 14 }
                            }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 10

                            Rectangle {
                                id: alcCalBtn
                                property bool calibrating: bridge.alcCalibrating
                                width: 220; height: controlHeight; radius: 4
                                color: calibrating
                                       ? (alcCalMA.containsMouse ? Qt.rgba(1,0.5,0,0.3) : bgMedium)
                                       : (alcCalMA.containsMouse ? Qt.rgba(1,0.6,0,0.3) : bgMedium)
                                border.color: calibrating ? "#ff9800" : "#ff9800"
                                ToolTip.visible: alcCalMA.containsMouse
                                ToolTip.delay: 600
                                ToolTip.text: qsTr("Transmits a tune carrier and auto-adjusts the TX audio level until the radio's ALC reaches the target. One-shot. Requires Hamlib CAT connected.")
                                Text {
                                    anchors.centerIn: parent
                                    text: alcCalBtn.calibrating
                                          ? qsTr("Cancel calibration")
                                          : qsTr("Calibrate ALC (transmits a carrier)")
                                    color: "#ff9800"
                                    font.pixelSize: 12
                                    font.bold: alcCalBtn.calibrating
                                }
                                MouseArea {
                                    id: alcCalMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (bridge.alcCalibrating)
                                            bridge.cancelAlcCalibration()
                                        else
                                            bridge.startAlcCalibration()
                                    }
                                }
                            }
                        }

                        // 1.0.325 — status label ALC: riga dedicata a tutta larghezza
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 4
                            Layout.minimumHeight: bridge.alcCalibrationStatus !== "" ? controlHeight : 0
                            visible: bridge.alcCalibrationStatus !== ""
                            spacing: 0
                            Text {
                                Layout.fillWidth: true
                                text: bridge.alcCalibrationStatus
                                color: bridge.alcCalibrating
                                       ? "#ff9800"
                                       : (bridge.alcCalibrationStatus.indexOf("Calibration done") >= 0
                                          ? accentGreen
                                          : "#f44336")
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.columnSpan: 4
                            Layout.preferredHeight: settingsDialog.scrollBottomMargin
                        }
                    }
                }

                // ═══════════ TAB 2 — AUDIO ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Dispositivi Audio ──
                        Text { text: qsTr("AUDIO DEVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 4 }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            Layout.preferredWidth: 110
                            Layout.preferredHeight: 28
                            Layout.alignment: Qt.AlignRight
                            radius: 6
                            color: audioRefreshMA.containsMouse ? bgMedium : "transparent"
                            border.color: glassBorder
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("↻  Refresh")
                                color: secondaryCyan
                                font.pixelSize: 11
                                font.bold: true
                            }
                            MouseArea {
                                id: audioRefreshMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.refreshAudioDevices()
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Input Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: audioInDevCombo
                            model: bridge.audioInputDevices
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            currentIndex: settingsDialog.stringListIndexOf(bridge.audioInputDevices, bridge.audioInputDevice)
                            onActivated: {
                                bridge.audioInputDevice = currentText
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.width: Math.max(audioInDevCombo.width, 560)
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Input Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: audioInChCombo
                            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            Layout.minimumWidth: fieldMinWidth
                            currentIndex: bridge.audioInputChannel
                            onActivated: {
                                bridge.audioInputChannel = currentIndex
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioInChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.columnSpan: 2 }

                        Text { text: qsTr("Output Device:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: audioOutDevCombo
                            model: bridge.audioOutputDevices
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            currentIndex: settingsDialog.stringListIndexOf(bridge.audioOutputDevices, bridge.audioOutputDevice)
                            onActivated: {
                                bridge.audioOutputDevice = currentText
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutDevCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.width: Math.max(audioOutDevCombo.width, 560)
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Output Channel:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: audioOutChCombo
                            model: [qsTr("Mono"),qsTr("Left"),qsTr("Right"),qsTr("Both")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            Layout.minimumWidth: fieldMinWidth
                            currentIndex: bridge.audioOutputChannel
                            onActivated: {
                                bridge.audioOutputChannel = currentIndex
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: audioOutChCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        // ── Livelli ──
                        Text { text: qsTr("LEVELS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("RX Input Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
	                        RowLayout {
	                            Layout.fillWidth: true
	                            Layout.columnSpan: 3
	                            spacing: 8
	                            Slider {
	                                id: setupRxInputLevelSlider
	                                from: 0; to: 100; live: true; stepSize: 1
	                                Layout.fillWidth: true
	                                Binding on value { value: bridge.rxInputLevel; when: !setupRxInputLevelSlider.pressed }
		                                onMoved: {
		                                    bridge.rxInputLevel = value
		                                    settingsDialog.scheduleSettingsPersist()
		                                }
		                                onPressedChanged: {
		                                    if (!pressed && Math.abs(bridge.rxInputLevel - value) >= 0.5) {
		                                        bridge.rxInputLevel = value
		                                        settingsDialog.scheduleSettingsPersist()
		                                    }
		                                }
	                            }
	                            Text {
	                                text: Math.round(bridge.rxInputLevel)
	                                color: secondaryCyan
	                                font.pixelSize: 11
	                                font.family: decodiumMonoFontFamily
	                                Layout.preferredWidth: 28
	                                horizontalAlignment: Text.AlignRight
	                            }
	                            Rectangle {
	                                Layout.preferredWidth: 48
	                                Layout.preferredHeight: 22
	                                radius: 4
	                                color: bridge.autoRxInputLevel
	                                       ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18)
	                                       : bgMedium
	                                border.color: bridge.autoRxInputLevel ? secondaryCyan : glassBorder
	                                Text {
	                                    anchors.centerIn: parent
	                                    text: "AUTO"
	                                    color: bridge.autoRxInputLevel ? secondaryCyan : textSecondary
	                                    font.pixelSize: 10
	                                    font.bold: true
	                                }
	                                MouseArea {
	                                    id: setupRxAutoMouse
	                                    anchors.fill: parent
	                                    hoverEnabled: true
	                                    cursorShape: Qt.PointingHandCursor
		                                    onClicked: {
		                                        bridge.autoRxInputLevel = !bridge.autoRxInputLevel
		                                        settingsDialog.scheduleSettingsPersist()
		                                    }
	                                }
	                                ToolTip.visible: setupRxAutoMouse.containsMouse
	                                ToolTip.text: bridge.autoRxInputLevel ? qsTr("Auto RX level active")
	                                                                       : qsTr("Auto RX level disabled")
	                            }
	                        }

                        Text { text: qsTr("TX Output Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        Slider {
                            id: setupTxOutputLevelSlider
                            from: 450; to: 0; live: true; stepSize: 1
                            Layout.fillWidth: true; Layout.columnSpan: 3
                            Binding on value { value: bridge.txOutputLevel; when: !setupTxOutputLevelSlider.pressed }
	                            onMoved: {
	                                bridge.txOutputLevel = value
	                                settingsDialog.scheduleSettingsPersist()
	                            }
	                            onPressedChanged: {
	                                if (!pressed && Math.abs(bridge.txOutputLevel - value) >= 0.5) {
	                                    bridge.txOutputLevel = value
	                                    settingsDialog.scheduleSettingsPersist()
	                                }
	                            }
                        }

                        // ── Directory ──
                        Text { text: qsTr("DIRECTORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Save Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            id: saveDirectoryField
                            text: bridge.getSetting("SaveDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            readOnly: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("SaveDirectory", text)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.openDirectoryPicker("SaveDirectory", saveDirectoryField.text)
                            }
                        }

                        Text { text: qsTr("AzEl Directory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoTextField {
                            id: azElDirectoryField
                            text: bridge.getSetting("AzElDirectory", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            topPadding: controlVerticalPadding; bottomPadding: controlVerticalPadding; verticalAlignment: TextInput.AlignVCenter
                            readOnly: true
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("AzElDirectory", text)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsDialog.openDirectoryPicker("AzElDirectory", azElDirectoryField.text)
                            }
                        }

                        // ── Power Memory ──
                        Text { text: qsTr("POWER MEMORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Band TX Memory:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTXMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTXMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Band Tune Mem:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PowerBandTuneMemory", false)
                            onCheckedChanged: bridge.setSetting("PowerBandTuneMemory", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 3 — TX ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Frequenza e Timing ──
                        Text { text: qsTr("FREQUENCY AND TIMING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("TX Frequency:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: txFreqSpin
                            from: 0; to: 5000; value: bridge.txFrequency; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: {
                                if (bridge.txFrequency !== value)
                                    bridge.txFrequency = value
                                bridge.setSetting("txFrequency", value)
                            }
                            contentItem: TextInput { text: txFreqSpin.textFromValue(txFreqSpin.value, txFreqSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txFreqSpin.editable; validator: txFreqSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("TX Slot:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: txSlotCombo
                            model: [qsTr("Second (:15/:45)"), qsTr("First (:00/:30)")]
                            currentIndex: bridge.txPeriod === 1 ? 1 : 0
                            Layout.fillWidth: true; Layout.minimumWidth: comboFieldMinWidth; Layout.preferredWidth: comboFieldMinWidth; implicitHeight: controlHeight
                            onActivated: {
                                bridge.txPeriod = currentIndex === 1 ? 1 : 0
                                bridge.setSetting("txPeriod", bridge.txPeriod)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: txSlotCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; rightPadding: 42; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("TX Delay (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: txDelaySpin
                            from: 0; to: 5; stepSize: 1; value: Math.round(Number(bridge.getSetting("TxDelay", 0.2)) * 10); editable: true
                            textFromValue: function(value, locale) { return Number(value / 10).toLocaleString(locale, "f", 1) }
                            valueFromText: function(text, locale) {
                                var parsed = Number.fromLocaleString(locale, text)
                                return isNaN(parsed) ? 0 : Math.round(parsed * 10)
                            }
                            validator: DoubleValidator { bottom: 0.0; top: 0.5; decimals: 1; notation: DoubleValidator.StandardNotation }
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: bridge.setSetting("TxDelay", value / 10)
                            contentItem: TextInput { text: txDelaySpin.textFromValue(txDelaySpin.value, txDelaySpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txDelaySpin.editable; validator: txDelaySpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Allow TX QSY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            checked: bridge.getSetting("TxQSYAllowed", false)
                            onCheckedChanged: bridge.setSetting("TxQSYAllowed", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Ready profiles (1.0.384) ──
                        // Apply a coherent set of FT2/decode toggles as a group.
                        // Selettore rapido equivalente anche in toolbar (accanto a Setup).
                        Text { text: qsTr("READY PROFILES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            implicitHeight: readyProfilesColumn.implicitHeight
                            ColumnLayout {
                                id: readyProfilesColumn
                                width: parent.width
                                spacing: 8
                                Repeater {
                                    model: [
                                        { pid: "balanced", name: qsTr("Balanced (daily QSO) - default"),
                                          desc: qsTr("Conservative ON · full decode AutoCQ ON · close strong partners ON · adaptive decode ON · AP cache rescue ON · skip end-slot OFF · MAM OFF · partner memory ON · TX2 resend ON · smooth flow ON · caller retries 5.") },
                                        { pid: "weak", name: qsTr("Weak-signal / DX hunting"),
                                          desc: qsTr("Like Balanced, but: caller retries 7 · adaptive decode OFF (maximum sensitivity) · AP cache rescue ON (accepts some false positives) · skip end-slot OFF (do not lose late decodes).") },
                                        { pid: "contest", name: qsTr("Contest / high density"),
                                          desc: qsTr("close strong partners ON · skip end-slot ON (minimum latency) · MAM multi-stream ON (2 streams, experimental) · full decode AutoCQ ON · caller retries 3 · partner memory ON · conservative OFF.") },
                                        { pid: "cpu", name: qsTr("CPU-limited (Decodium Console / mini PC)"),
                                          desc: qsTr("adaptive decode ON · MAM OFF · full decode AutoCQ OFF · smooth flow ON · rest at defaults. Watchdogs unchanged.") }
                                    ]
                                    delegate: Rectangle {
                                        id: profileEntry
                                        required property var modelData
                                        readonly property bool isActive: bridge && bridge.activeReadyProfile === modelData.pid
                                        Layout.fillWidth: true
                                        implicitHeight: profileEntryCol.implicitHeight + 16
                                        radius: 6
                                        color: isActive ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18) : bgMedium
                                        border.color: isActive ? primaryBlue : glassBorder
                                        border.width: 1
                                        ColumnLayout {
                                            id: profileEntryCol
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.margins: 8
                                            spacing: 3
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6
                                                Text { text: modelData.name; color: textPrimary; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                                Text { text: qsTr("● active"); color: primaryBlue; font.pixelSize: 11; font.bold: true; visible: profileEntry.isActive }
                                            }
                                            Text { text: modelData.desc; color: textSecondary; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: if (bridge) bridge.applyReadyProfile(modelData.pid)
                                        }
                                    }
                                }
                            }
                        }

                        // ── Sequenza Automatica ──
                        Text { text: qsTr("AUTO SEQUENCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            implicitHeight: autoSequenceGrid.implicitHeight

                            GridLayout {
                                id: autoSequenceGrid
                                width: parent.width
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 10
                                property int checkWidth: 34
                                property int valueWidth: 110
                                property real labelWidth: Math.max(240, width - valueWidth - columnSpacing)

                                Text {
                                    text: qsTr("Auto Sequence:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge.autoSeq
                                    onCheckedChanged: {
                                        bridge.autoSeq = checked
                                        bridge.setSetting("autoSeq", checked)
                                        bridge.setSetting("AutoSeq", checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text {
                                    text: qsTr("Send RR73:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge.sendRR73
                                    onCheckedChanged: {
                                        bridge.sendRR73 = checked
                                        bridge.setSetting("sendRR73", checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text {
                                    text: qsTr("Quick QSO:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge.quickQsoEnabled
                                    onCheckedChanged: {
                                        bridge.quickQsoEnabled = checked
                                        bridge.setSetting("quickQsoEnabled", checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                // 1.0.304 (#9) — resume-on-reply: riprende il QSO se il partner
                                // torna a rispondere entro 2 min dall'Halt. Opt-in, default OFF.
                                Text {
                                    text: qsTr("Resume QSO on partner reply:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.resumeQsoOnReply : false
                                    onCheckedChanged: {
                                        if (bridge) {
                                            bridge.resumeQsoOnReply = checked
                                            settingsDialog.scheduleSettingsPersist()
                                        }
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("If you Halt during an active QSO and that same station sends a direct reply to your callsign within 2 minutes, Decodium can resume that QSO.\n\nApplies only to FT8/FT4/FT2 and only to the saved QSO state.\n\nDefault: OFF (= Halt fully stops the sequence by default).")
                                }
                                Text {
                                    text: qsTr("Disable TX after 73:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge.getSetting("73TxDisable", true)
                                    onCheckedChanged: bridge.setSetting("73TxDisable", checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text {
                                    text: qsTr("MSK/Q65 TX until 73:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge.getSetting("RepeatTx", false)
                                    onCheckedChanged: bridge.setSetting("RepeatTx", checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                // ══════════ FT2 UTILITY ══════════
                                Text { text: qsTr("FT2 UTILITY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 10 }
                                Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                                // 1.0.311 — FT2: ripetizioni del 73/RR73 finale regolabili (era fisso 8)
                                Text {
                                    text: qsTr("FT2: signoff retries (73/RR73):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                SpinBox {
                                    id: ft2SignoffCapSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 1; to: 8; editable: true
                                    value: bridge ? bridge.ft2SignoffRetryCap : 4
                                    onValueChanged: if (bridge && bridge.ft2SignoffRetryCap !== value) bridge.setFt2SignoffRetryCap(value)
                                    contentItem: TextInput { text: ft2SignoffCapSpin.textFromValue(ft2SignoffCapSpin.value, ft2SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft2SignoffCapSpin.editable; validator: ft2SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT2 waiting for the partner's ack before logging and closing.\n\nDefault: 4 (~28s).\n\nLower = closes earlier (less 'stuck' on the same station).\nHigher = more patient with weak/QSB partners.\n\nDoesn't affect FT8/FT4.")
                                }

                                // 1.0.315 — FT4: ripetizioni del 73/RR73 finale regolabili
                                Text {
                                    text: qsTr("FT4: signoff retries (73/RR73):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                SpinBox {
                                    id: ft4SignoffCapSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 1; to: 8; editable: true
                                    value: bridge ? bridge.ft4SignoffRetryCap : 4
                                    onValueChanged: if (bridge && bridge.ft4SignoffRetryCap !== value) bridge.setFt4SignoffRetryCap(value)
                                    contentItem: TextInput { text: ft4SignoffCapSpin.textFromValue(ft4SignoffCapSpin.value, ft4SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft4SignoffCapSpin.editable; validator: ft4SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT4.\n\nDefault: 4 (~30s).\n\nIncrease to 6-8 for weak/QSB partners (replaces the former automatic weak/conservative extras).\n\nDoesn't affect FT2/FT8.")
                                }

                                // 1.0.315 — FT8: ripetizioni del 73/RR73 finale regolabili
                                Text {
                                    text: qsTr("FT8: signoff retries (73/RR73):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                SpinBox {
                                    id: ft8SignoffCapSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 1; to: 8; editable: true
                                    value: bridge ? bridge.ft8SignoffRetryCap : 3
                                    onValueChanged: if (bridge && bridge.ft8SignoffRetryCap !== value) bridge.setFt8SignoffRetryCap(value)
                                    contentItem: TextInput { text: ft8SignoffCapSpin.textFromValue(ft8SignoffCapSpin.value, ft8SignoffCapSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft8SignoffCapSpin.editable; validator: ft8SignoffCapSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many times to repeat the final 73/RR73 in FT8.\n\nDefault: 3 (~45s).\n\nIncrease to 6-8 for weak/QSB partners (replaces the former automatic weak/conservative extras).\n\nDoesn't affect FT2/FT4.")
                                }

                                // 1.0.437 - opt-in: extra signoff retries per partner debole (FTX)
                                Text {
                                    text: qsTr("Weak-partner signoff boost (FT2/4/8):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ftxWeakBoostCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ftxWeakSignoffBoost : false
                                    onCheckedChanged: if (bridge && bridge.ftxWeakSignoffBoost !== checked) bridge.setFtxWeakSignoffBoost(checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("When ON, automatically grants extra final 73/RR73 retries when the active partner is weak (SNR at or below the threshold below), giving fragile QSOs more chances to close.\n\nDefault OFF = unchanged behavior. Applies to FT2/FT4/FT8, always clamped to max 8 and still bounded by the TX watchdog.")
                                }
                                Text {
                                    text: qsTr("  weak SNR threshold (dB):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                    enabled: ftxWeakBoostCheck.checked
                                }
                                SpinBox {
                                    id: ftxWeakSnrSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: -30; to: -5; editable: true
                                    enabled: ftxWeakBoostCheck.checked
                                    value: bridge ? bridge.ftxWeakSnrThreshold : -15
                                    onValueChanged: if (bridge && bridge.ftxWeakSnrThreshold !== value) bridge.setFtxWeakSnrThreshold(value)
                                    contentItem: TextInput { text: ftxWeakSnrSpin.textFromValue(ftxWeakSnrSpin.value, ftxWeakSnrSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ftxWeakSnrSpin.editable; validator: ftxWeakSnrSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("A partner whose SNR is at or below this value is treated as 'weak' and receives the extra signoff retries.\n\nDefault: -15 dB.")
                                }
                                Text {
                                    text: qsTr("  extra signoff retries:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                    enabled: ftxWeakBoostCheck.checked
                                }
                                SpinBox {
                                    id: ftxWeakBonusSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 1; to: 6; editable: true
                                    enabled: ftxWeakBoostCheck.checked
                                    value: bridge ? bridge.ftxWeakSignoffBonus : 3
                                    onValueChanged: if (bridge && bridge.ftxWeakSignoffBonus !== value) bridge.setFtxWeakSignoffBonus(value)
                                    contentItem: TextInput { text: ftxWeakBonusSpin.textFromValue(ftxWeakBonusSpin.value, ftxWeakBonusSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ftxWeakBonusSpin.editable; validator: ftxWeakBonusSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many extra final 73/RR73 retries to add on top of the per-mode cap for weak partners.\n\nDefault: +3 (capped so the total never exceeds 8).")
                                }

                                // 1.0.446 - opt-in: guard ri-aggancio RRR post-log (caso 9H1SR "troppe richiamate")
                                Text {
                                    text: qsTr("Post-log RRR re-engage guard (FT2):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2ReengageGuardCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2PostLogReengageGuard : false
                                    onCheckedChanged: if (bridge && bridge.ft2PostLogReengageGuard !== checked) bridge.setFt2PostLogReengageGuard(checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("When ON, after a QSO is logged ('partner left') stop re-sending RR73 to a partner that keeps calling you with R+report because they did not copy your signoff (the 9H1SR too-many-calls case).\n\nA few courtesy repeats are still allowed (see max), then suppressed within the 30s cooldown. Default OFF. FT2 only.")
                                }
                                Text {
                                    text: qsTr("  courtesy RRR max:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                    enabled: ft2ReengageGuardCheck.checked
                                }
                                SpinBox {
                                    id: ft2ReengageMaxSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 0; to: 5; editable: true
                                    enabled: ft2ReengageGuardCheck.checked
                                    value: bridge ? bridge.ft2PostLogReengageMax : 1
                                    onValueChanged: if (bridge && bridge.ft2PostLogReengageMax !== value) bridge.setFt2PostLogReengageMax(value)
                                    contentItem: TextInput { text: ft2ReengageMaxSpin.textFromValue(ft2ReengageMaxSpin.value, ft2ReengageMaxSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !ft2ReengageMaxSpin.editable; validator: ft2ReengageMaxSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many courtesy RR73 to still send to a just-logged partner before suppressing further re-engagements.\n\n0 = suppress immediately. Default: 1.")
                                }

                                // 1.0.314 — opt-in: TX immediato al click (stile 1.0.283)
                                Text {
                                    text: qsTr("Immediate TX on click (1.0.283 style):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ftxImmediateClickCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ftxImmediateClickTx : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ftxImmediateClickTx !== checked)
                                            bridge.setFtxImmediateClickTx(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Restores the 'TX starts IMMEDIATELY on double-click' behaviour of 1.0.283.\n\n• FT2: relaxes the period-gate (TX1 from click bypasses waiting for the next slot)\n• FT8/FT4: raises the clickable window cap to d3CapMs (~11s on FT8, 5.6s on FT4) = real 1.0.283 behaviour\n\nDefault: OFF (= safe upstream behaviour).\n\nEnable if it bothers you to wait 1 cycle after the click.")
                                }

                                // 1.0.371 - opt-in: logga RR73 (TX4) anche se il partner sparisce (FT2 async AutoCQ)
                                Text {
                                    text: qsTr("Log RR73 even if partner leaves (FT2):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2LogRr73OnPartnerLeftCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2LogRr73OnPartnerLeft : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft2LogRr73OnPartnerLeft !== checked)
                                            bridge.setFt2LogRr73OnPartnerLeft(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("FT2 + async AutoCQ: when WE close with RR73 (TX4) after the partner R+report and the partner then disappears, log the QSO anyway (at the signoff cap) instead of leaving it unlogged.\n\nMatches TX5/73 and sync mode behaviour.\n\nDefault: OFF.")
                                }

                                // 1.0.317 — opt-in: FT8 fast sequence (grace ridotta + late-decode accept)
                                Text {
                                    text: qsTr("FT8: fast sequences (WSJT-X/JTDX style):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft8FastSequenceCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft8FastSequence : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft8FastSequence !== checked)
                                            bridge.setFt8FastSequence(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Reduces FT8 sequence waits for users who prefer WSJT-X/JTDX-style reactivity.\n\nTwo changes:\n  (1) Boundary grace 1200ms → 400ms = TX starts ~800ms earlier after the slot boundary\n  (2) onFt8DecodeReady accepts late decodes within d3CapMs (~11s) instead of dropping the slot = no more '15s extra after the partner's reply'\n\nSAFETY: under CPU pressure the pre-existing clamp forces grace ≥900ms (safety > reactivity on loaded PCs).\n\nDefault: OFF (= conservative upstream behaviour, max decode reliability).")
                                }

                                // 1.0.367 — opt-in: finestra TX FT2 async conservativa (default ON = calmo/stabile)
                                Text {
                                    text: qsTr("FT2: conservative TX window (no truncated frames):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2ConservativeTimingCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2ConservativeTiming : true
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft2ConservativeTiming !== checked)
                                            bridge.setFt2ConservativeTiming(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Controls how late in a slot the async FT2 TX may start.\n\n• ON (default): the TX starts only if the FULL payload (~2520ms) still fits — window ~18% of the slot. If it would arrive late, the TX is deferred to the next slot instead of sending a TRUNCATED frame the partner can't decode. Calm, Decodium-3.0-style stability.\n• OFF: FIX B (1.0.353) behaviour — window up to ~76% of the slot (only ~700ms of useful payload required). More reactive but can transmit truncated frames on a late reply.\n\nEnable OFF only if you want maximum reactivity and accept occasional non-decodable late TX.")
                                }

                                // 1.0.321 — opt-in: FT2 manual one-shot disarm (Salvatore 1.0.300 latch fix)
                                Text {
                                    text: qsTr("FT2: manual one-shot disarm (1.0.300+):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2ManualOneShotCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2ManualOneShotEnabled : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft2ManualOneShotEnabled !== checked)
                                            bridge.setFt2ManualOneShotEnabled(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("When ON (upstream 1.0.300+ behaviour): after a manual TX1-TX3 in FT2 the TX is disarmed and re-armed ONLY when a partner decode arrives. Avoids TX1 looping forever on double-click, but on WEAK partners that don't decode in the first RX period the QSO is lost (= 'TX1 stops without completing').\n\nWhen OFF (default on this fork, pre-1.0.300): TX1 keeps repeating until 'Caller Retries' is reached — better for weak-signal QSOs (Pasquale's case).\n\nEnable only if you double-click stations that consistently reply on the first attempt.")
                                }

                                // 1.0.321 — Caller retries (era Q_PROPERTY non esposta in UI)
                                Text {
                                    text: qsTr("Caller retries (max TX repeats per step):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.columnSpan: 2
                                    Layout.preferredHeight: Math.max(controlHeight, implicitHeight)
                                }
                                SpinBox {
                                    id: maxCallerRetriesSpin
                                    Layout.columnSpan: 2
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 1; to: 99; editable: true
                                    value: bridge ? bridge.maxCallerRetries : 10
                                    onValueChanged: if (bridge && bridge.maxCallerRetries !== value) bridge.setMaxCallerRetries(value)
                                    contentItem: TextInput { text: maxCallerRetriesSpin.textFromValue(maxCallerRetriesSpin.value, maxCallerRetriesSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !maxCallerRetriesSpin.editable; validator: maxCallerRetriesSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Maximum times the same TX step (TX1/TX2/TX3) repeats before halting if the partner doesn't reply.\n\nDefault: 10.\n\nFT2 (slot 3.75s): 10 retries ≈ 38s of calling.\nFT8 (slot 15s): 10 retries ≈ 150s.\n\nLower (4-6) = less time wasted on stations that don't reply.\nHigher (15-20) = patience for weak DX / marginal propagation.\n\nNote: with 'FT2 manual one-shot disarm' OFF (default) this is what stops TX1 from looping forever.")
                                }

                                // 1.0.446 - P1-5 opt-in: cap Caller retries duro anche con TX watchdog ON
                                Text {
                                    text: qsTr("Caller retries hard cap (even with watchdog):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: callerRetriesAlwaysHardCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.callerRetriesAlwaysHard : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.callerRetriesAlwaysHard !== checked)
                                            bridge.setCallerRetriesAlwaysHard(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("When ON, the 'Caller retries' cap on TX1/TX2 halts the call even if the TX Watchdog is enabled.\n\nDefault OFF (1.0.438 behaviour): when the TX Watchdog is ON it takes priority and ignores the Caller-retries cap until its own timeout, so a call can repeat for the whole watchdog duration.\n\nEnable for a hard limit on TX repeats regardless of the watchdog.")
                                }

                                // 1.0.447 - Fondamenta Fase 1: censimento transizioni di stato FT2 (diagnostico, solo-log)
                                Text {
                                    text: qsTr("FT2 state transition census (log only):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2TransitionCensusCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2TransitionCensus : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft2TransitionCensus !== checked)
                                            bridge.setFt2TransitionCensus(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Diagnostic only (off by default): logs every FT2 QSO state transition (from/to/progress) to the diagnostic log, to empirically map the real sequencer state machine. No behaviour change at all - it only writes log lines. Used to design future deterministic-transition guards safely.")
                                }

                                // 1.0.447 - Leva#6-A: gate smart-TX adattivi all'occupazione canale (sperimentale)
                                Text {
                                    text: qsTr("Adaptive async TX timing (experimental):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2AdaptiveTxGatesCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2AdaptiveTxGates : false
                                    onCheckedChanged: {
                                        if (bridge && bridge.ft2AdaptiveTxGates !== checked)
                                            bridge.setFt2AdaptiveTxGates(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Experimental, off by default. Makes the FT2 async-TX timing gates (RMS-quiet, decode-quiet, anti-collision jitter) adapt to channel occupancy: a bit more reactive when the channel is clear, more conservative when it is crowded. With this OFF the timing is byte-identical to the standard behaviour. It never transmits before hearing the partner.")
                                }

                                // Conservative FT2 (weak-signal mode) — opt-in tuning
                                // anti-QSB: ghost filter rilassato, retry cap esteso SNR-
                                // adattivo, same-step wait piu' permissivo per partner
                                // marginali. Default OFF: comportamento standard FT2.
                                Text {
                                    text: qsTr("Conservative FT2 (weak-signal mode):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2ConservativeCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2Conservative : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2Conservative(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Anti-QSB tuning:\n  • Ghost filter -24 dB instead of -22\n  • Retry cap extended SNR-adaptive (+2..+4 extra)\n  • Same-step wait relaxed for weak partners\n\nDefault: OFF — enable it if you have weak DX partners or marginal propagation.")
                                }

                                // 1.0.289 — FT2 #1: piena profondità decode durante AutoCQ
                                Text {
                                    text: qsTr("FT2: full decode in AutoCQ:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2FullDecodeCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2FullDecodeInAutoCq : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2FullDecodeInAutoCq(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("While calling CQ (AutoCQ), keeps the decode depth at full (OSD + 4th subtraction pass + weak-signal averaging) instead of reducing it to 2.\n\nHelps you hear weak responders. Reduces automatically under CPU pressure anyway.\n\nDefault: OFF.")
                                }

                                // 1.0.289 — FT2 #3: chiusura rapida partner forti
                                Text {
                                    text: qsTr("FT2: close strong partners earlier:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2QuickGiveUpCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2QuickGiveUpStrong : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2QuickGiveUpStrong(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("If a STRONG partner (SNR > 0 dB) doesn't send the final 73, reduces RR73 repetitions from 8 to 4 (~15s instead of 30s) before logging and returning to CQ.\n\nWeak partners keep the extra anti-QSB repetitions.\n\nDefault: OFF.")
                                }

                                // 1.0.292 — FT2: decode adattivo (dedup re-decode async in solo-ascolto)
                                Text {
                                    text: qsTr("FT2: adaptive decode (CPU saver):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2AdaptiveDecodeCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2AdaptiveDecode : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2AdaptiveDecode(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("In LISTEN-ONLY mode (not calling CQ nor in a QSO), thins async re-decode from 100ms to ~350ms: doesn't re-decode 95%-overlapping audio → saves CPU and reduces the peaks that may lower decode depth.\n\nWhen waiting for a reply (AutoCQ/QSO) it stays at full cadence. Loses no decodes.\n\nUseful mainly on modest PCs.\n\nDefault: OFF.")
                                }

                                // Sprint2-1 — FT2: narrow async decode (fast pass attorno a nfqso)
                                Text {
                                    text: qsTr("FT2: narrow reply decode (experimental):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2NarrowAsyncDecodeCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2NarrowAsyncDecode : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2NarrowAsyncDecode(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("When WAITING FOR A REPLY (AutoCQ or active QSO), decodes a narrow window around your RX frequency (±150 Hz) instead of the whole band, with a full-band pass every 4th cycle.\n\nThe reply is decoded earlier in the slot (less CPU per attempt), so TX can react in the same slot instead of the next one. Band activity is still scanned 1 cycle out of 4.\n\nDefault: OFF.")
                                }

                                // 1.0.293/294 — FT2: AP hashed-callsign cache (Phase 1: display-only rescue)
                                Text {
                                    text: qsTr("FT2: AP cache rescue (experimental):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2ApHashCacheCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2ApHashCache : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2ApHashCache(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Experimental FT2 AP cache: stores callsigns seen in-band as hashes (TTL 30 min) and may rescue borderline FT2 decodes when a decoded callsign is already in the cache.\n\nSafety gate: AP-cache-rescued rows are shown/audited, but they do not drive AutoSeq, AutoCQ, or automatic TX. They are also not used to seed the AP cache again.\n\nDefault: OFF.")
                                }

                                // 1.0.355 — FT2: salta decode ridondante di fine-slot
                                Text {
                                    text: qsTr("FT2: skip redundant end-slot decode (reduces lock-in latency):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2AsyncSkipRedundantSyncDecodeCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2AsyncSkipRedundantSyncDecode : false
                                    onToggled: {
                                        if (bridge) {
                                            bridge.ft2AsyncSkipRedundantSyncDecode = checked
                                            settingsDialog.scheduleSettingsPersist()
                                        }
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("FT2 async only: when the asynchronous decode (incremental every 100 ms) has ALREADY decoded a slot, skip the end-of-slot synchronous decode pass for that slot.\n\nBenefit: removes contention (~1.8 s after TX) on the same worker, so the partner reply is picked up faster.\n\nCost: for slots already covered by async you lose the full end-of-slot weak-averaging pass, which can recover weak/marginal stations. Slots where async returned EMPTY still keep the sync decode.\n\nDefault: OFF.")
                                }

                                // 1.0.364+ — MAM multi-stream (MSHV): risponde a più
                                // chiamanti nello stesso slot su frequenze diverse.
                                // Opzione AGGIUNTIVA del MAM seriale. Default OFF.
                                Text {
                                    text: qsTr("FT2/FT8 MAM multi-stream (MSHV, sperimentale):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: mamMultiStreamCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.mamMultiStream : false
                                    onToggled: {
                                        if (bridge) {
                                            bridge.mamMultiStream = checked
                                            settingsDialog.scheduleSettingsPersist()
                                        }
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("MSHV multi-stream mode: in a single period it replies to MULTIPLE callers at the same time, each on ITS own audio frequency (like a DX-pedition station).\n\nThis is an ADDITIONAL MAM option: MAM (Multi-Answer Mode) or AutoCQ must be active before it can run. With this OFF, MAM remains serial, one caller at a time, as before.\n\nEXPERIMENTAL. Default: OFF.")
                                }

                                // 1.0.364+ — MAM multi-stream: numero massimo di stream simultanei
                                Text {
                                    text: qsTr("MAM multi-stream: max stream simultanei:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                SpinBox {
                                    id: mamMaxStreamsSpin
                                    Layout.preferredWidth: autoSequenceGrid.valueWidth
                                    Layout.alignment: Qt.AlignLeft
                                    implicitHeight: controlHeight
                                    from: 2; to: 5; editable: true
                                    enabled: bridge ? bridge.mamMultiStream : false
                                    opacity: enabled ? 1.0 : 0.4
                                    value: bridge ? bridge.mamMaxStreams : 3
                                    onValueModified: {
                                        if (bridge && bridge.mamMaxStreams !== value) {
                                            bridge.mamMaxStreams = value
                                            settingsDialog.scheduleSettingsPersist()
                                        }
                                    }
                                    contentItem: TextInput { text: mamMaxStreamsSpin.textFromValue(mamMaxStreamsSpin.value, mamMaxStreamsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !mamMaxStreamsSpin.editable; validator: mamMaxStreamsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                    background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("How many parallel QSOs MAM multi-stream can run at the same time, each on its own frequency.\n\nRange 2-5. Default: 3.\n\nHigher values require more CPU to generate overlapping audio streams. Enabled only when MAM multi-stream is active.")
                                }

                                // 1.0.187 — FT2 Weak-Signal Pack F v2: partner-memory cache (30s)
                                Text {
                                    text: qsTr("FT2 partner-memory (anti-QSB):")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2PartnerMemoryCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2PartnerMemoryEnabled : false
                                    enabled: bridge ? bridge.ft2Conservative : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2PartnerMemoryEnabled(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2; opacity: parent.enabled ? 1.0 : 0.4 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Caches partner state (callsign + TX num + qsoProgress + SNR) for 30 seconds: if the partner disappears for QSB and reappears within 30s, restores the qsoProgress instead of restarting from TX1.\n\nRequires Conservative FT2 active.\n\nDefault: OFF (opt-in after the 1.0.186 revert — strict gate + [FT2WS-F] log). Automatically disabled if Conservative is OFF.")
                                }

                                // 1.0.187 — FT2 Weak-Signal Pack G: TX2 re-send forzato pre-fallback
                                Text {
                                    text: qsTr("FT2 TX2 re-send on stall:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: ft2Tx2ResendCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.ft2Tx2ResendOnStall : true
                                    enabled: bridge ? bridge.ft2Conservative : false
                                    onCheckedChanged: {
                                        if (bridge) bridge.setFt2Tx2ResendOnStall(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2; opacity: parent.enabled ? 1.0 : 0.4 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("If you're in TX3 (R+report) and the partner doesn't reply for 2 periods (~7.5s), re-sends TX2 (signal report) once before leaving the QSO.\n\nHelps with weak partners that didn't ack the first time. Capped to 1 re-send per QSO (no loops).\n\nRequires Conservative FT2 active. Default: ON under Conservative.")
                                }

                                // Smooth decode flow (streaming progressivo FT8/FT4)
                                // — spalma i decode dal batch a streaming continuo
                                // stile WSJT-X live. Auto-fallback se UI stall.
                                // Default ON; disattiva se vedi rallentamenti.
                                Text {
                                    text: qsTr("Smooth decode flow:")
                                    color: textSecondary
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    verticalAlignment: Text.AlignVCenter
                                    Layout.preferredWidth: autoSequenceGrid.labelWidth
                                    Layout.preferredHeight: controlHeight
                                }
                                CheckBox {
                                    id: smoothDecodeFlowCheck
                                    Layout.preferredWidth: autoSequenceGrid.checkWidth
                                    Layout.preferredHeight: controlHeight
                                    checked: bridge ? bridge.smoothDecodeFlow : true
                                    onCheckedChanged: {
                                        if (bridge) bridge.setSmoothDecodeFlow(checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Spreads FT8/FT4 decodes from the final end-of-period batch into continuous streaming with animated fade (~100 ms per row). FT2 async is unchanged because it already streams. Default: ON; auto-fallback if UI stalls are detected on modest PCs. Disable for legacy batch behavior.")
                                }
                            }
                        }

                        // ── Watchdog ──
                        Text { text: qsTr("WATCHDOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("TX Watchdog Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: txWdModeCombo
                            model: [qsTr("Off"), qsTr("Time"), qsTr("Count")]
                            currentIndex: bridge ? bridge.txWatchdogMode : 0
                            implicitHeight: controlHeight
                            Layout.fillWidth: true
                            Layout.minimumWidth: numericFieldMinWidth
                            Layout.preferredWidth: numericFieldMinWidth
                            onActivated: {
                                if (bridge && bridge.txWatchdogMode !== currentIndex) {
                                    bridge.txWatchdogMode = currentIndex
                                    settingsDialog.scheduleSettingsPersist()
                                }
                            }
                            contentItem: Text {
                                text: parent.displayText
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Text { text: qsTr("TX Watchdog Time (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: txWdSpin
                            from: 1; to: 999; value: bridge.txWatchdogTime; editable: true
                            enabled: bridge.txWatchdogMode === 1
                            property bool completed: false
                            function applyWatchdog() {
                                if (bridge && bridge.txWatchdogMode === 1 && bridge.txWatchdogTime !== value) {
                                    bridge.txWatchdogTime = value
                                    settingsDialog.scheduleSettingsPersist()
                                }
                            }
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: if (completed) applyWatchdog()
                            Component.onCompleted: completed = true
                            contentItem: TextInput { text: txWdSpin.textFromValue(txWdSpin.value, txWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txWdSpin.editable; validator: txWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("TX Watchdog Count:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: txWdCountSpin
                            from: 1; to: 50; value: bridge.txWatchdogCount; editable: true
                            enabled: bridge.txWatchdogMode === 2
                            property bool completed: false
                            function applyWatchdog() {
                                if (bridge && bridge.txWatchdogMode === 2 && bridge.txWatchdogCount !== value) {
                                    bridge.txWatchdogCount = value
                                    settingsDialog.scheduleSettingsPersist()
                                }
                            }
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: if (completed) applyWatchdog()
                            Component.onCompleted: completed = true
                            contentItem: TextInput { text: txWdCountSpin.textFromValue(txWdCountSpin.value, txWdCountSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !txWdCountSpin.editable; validator: txWdCountSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        // 1.0.446 - P0-3 opt-in: logga il QSO se il watchdog scatta a scambio completato
                        Text { text: qsTr("Log QSO at watchdog timeout:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: txWdLogOnCloseCheck
                            implicitHeight: controlHeight
                            checked: bridge ? bridge.txWatchdogLogOnClose : false
                            onCheckedChanged: if (bridge && bridge.txWatchdogLogOnClose !== checked) bridge.setTxWatchdogLogOnClose(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("When ON, if the TX watchdog fires while a QSO has already completed the two-way report exchange (both reports exchanged, progress >= ROGER_REPORT), the QSO is logged instead of abandoned.\n\nDefault OFF = 1.0.445 behavior (only a deferred snapshot, recovered only if the partner re-sends 73; in a manual QSO it is lost).")
                        }
                        Text { text: qsTr("Tune Watchdog (s):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        RowLayout {
                            Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth + 44; Layout.preferredWidth: numericFieldMinWidth + 44; spacing: 6
                            CheckBox {
                                id: tuneWdCheck
                                checked: bridge.getSetting("TuneWatchdog", true)
                                onCheckedChanged: bridge.setSetting("TuneWatchdog", checked)
                                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                contentItem: Text { text: ""; leftPadding: 24 }
                            }
                            SpinBox {
                                id: tuneWdSpin
                                from: 0; to: 300; value: Number(bridge.getSetting("TuneWatchdogTime", 90)); editable: true; enabled: tuneWdCheck.checked
                                implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                                onValueChanged: bridge.setSetting("TuneWatchdogTime", value)
                                contentItem: TextInput { text: tuneWdSpin.textFromValue(tuneWdSpin.value, tuneWdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !tuneWdSpin.editable; validator: tuneWdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            }
                        }

                        // ── CW ID ──
                        Text { text: qsTr("CW ID"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("CW ID after 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("After73", false)
                            onCheckedChanged: bridge.setSetting("After73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("CW ID Interval (min):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cwIdIntSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("IDint", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("IDint", value)
                            contentItem: TextInput { text: cwIdIntSpin.textFromValue(cwIdIntSpin.value, cwIdIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !cwIdIntSpin.editable; validator: cwIdIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Tone Spacing ──
                        Text { text: qsTr("TONE SPACING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("2x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: x2ToneSpacingCheck
                            checked: bridge.getSetting("x2ToneSpacing", false)
                            onCheckedChanged: {
                                if (checked) {
                                    x4ToneSpacingCheck.checked = false
                                    bridge.setSetting("x4ToneSpacing", false)
                                }
                                bridge.setSetting("x2ToneSpacing", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("4x Tone Spacing:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: x4ToneSpacingCheck
                            checked: bridge.getSetting("x4ToneSpacing", false)
                            onCheckedChanged: {
                                if (checked) {
                                    x2ToneSpacingCheck.checked = false
                                    bridge.setSetting("x2ToneSpacing", false)
                                }
                                bridge.setSetting("x4ToneSpacing", checked)
                            }
                            Component.onCompleted: {
                                if (checked && x2ToneSpacingCheck.checked) {
                                    x2ToneSpacingCheck.checked = false
                                    bridge.setSetting("x2ToneSpacing", false)
                                }
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Alt F1-F6 Bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("AlternateBindings", false)
                            onCheckedChanged: bridge.setSetting("AlternateBindings", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 4 — DISPLAY ═══════════
                ScrollView {
                    id: displaySettingsScroll
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    contentWidth: availableWidth
                    contentHeight: displaySettingsGrid.implicitHeight + 28

                    GridLayout {
                        id: displaySettingsGrid
                        width: Math.max(0, displaySettingsScroll.availableWidth - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Aspetto / Tema ──
                        Text { text: qsTr("ASPETTO / TEMA"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Theme:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: themeCombo
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            model: bridge.themeManager.availableThemes
                            currentIndex: Math.max(0, model.indexOf(bridge.themeManager.currentTheme))
                            onActivated: bridge.themeManager.applyThemeByName(currentText)
                            Connections {
                                target: bridge.themeManager
                                function onCurrentThemeChanged() {
                                    var i = themeCombo.model.indexOf(bridge.themeManager.currentTheme)
                                    if (i >= 0 && themeCombo.currentIndex !== i)
                                        themeCombo.currentIndex = i
                                }
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate {
                                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
                            }
                        }
                        // riga vuota per riempire le 4 colonne
                        Item { Layout.columnSpan: 2; Layout.preferredHeight: controlHeight }

                        // DX-Pedition Fase 1 — Accent + Density (visibili solo col tema DX-Pedition)
                        Text {
                            text: qsTr("Accent:")
                            color: textSecondary; font.pixelSize: 12
                            Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight
                            verticalAlignment: Text.AlignVCenter
                            visible: bridge.themeManager.currentTheme === "Darkcodium"
                        }
                        RowLayout {
                            id: dxpAccentRow
                            Layout.columnSpan: 3; Layout.fillWidth: true
                            spacing: 8
                            visible: bridge.themeManager.currentTheme === "Darkcodium"
                            readonly property var accents: [
                                { key: "phosphor", color: "#19ff88" },
                                { key: "cyan",     color: "#66e6ff" },
                                { key: "amber",    color: "#ffb820" },
                                { key: "red",      color: "#ff5466" }
                            ]
                            Repeater {
                                model: dxpAccentRow.accents
                                delegate: Rectangle {
                                    Layout.preferredWidth: 34; Layout.preferredHeight: controlHeight
                                    radius: 6
                                    color: modelData.color
                                    readonly property bool sel: bridge.themeManager.accentVariant === modelData.key
                                    border.width: sel ? 2 : 1
                                    border.color: sel ? textPrimary : glassBorder
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: bridge.themeManager.accentVariant = modelData.key
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            text: qsTr("Density:")
                            color: textSecondary; font.pixelSize: 12
                            Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight
                            verticalAlignment: Text.AlignVCenter
                            visible: bridge.themeManager.currentTheme === "Darkcodium"
                        }
                        RowLayout {
                            id: dxpDensityRow
                            Layout.columnSpan: 3; Layout.fillWidth: true
                            spacing: 0
                            visible: bridge.themeManager.currentTheme === "Darkcodium"
                            readonly property var densities: ["compact", "regular", "comfy"]
                            Repeater {
                                model: dxpDensityRow.densities
                                delegate: Rectangle {
                                    Layout.preferredWidth: 86; Layout.preferredHeight: controlHeight
                                    readonly property bool sel: bridge.themeManager.density === modelData
                                    color: sel ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.30) : bgMedium
                                    border.color: glassBorder
                                    border.width: 1
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                                        color: parent.sel ? textPrimary : textSecondary
                                        font.pixelSize: controlFontSize
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: bridge.themeManager.density = modelData
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        // DX-Pedition Fase 2a — opt-in 3-column tactical workspace toggle.
                        CheckBox {
                            id: dxPeditionWorkspaceCheck
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            text: qsTr("DX-Pedition Workspace (3-column tactical layout)")
                            checked: mainWindow.dxPeditionMode
                            onToggled: {
                                mainWindow.dxPeditionMode = checked
                                bridge.setSetting("uiDxPeditionMode", checked)
                            }
                            contentItem: Text {
                                text: dxPeditionWorkspaceCheck.text
                                color: textPrimary
                                font.pixelSize: 12
                                leftPadding: dxPeditionWorkspaceCheck.indicator.width + 8
                                verticalAlignment: Text.AlignVCenter
                            }
                            ToolTip.visible: hovered
                            ToolTip.delay: 600
                            ToolTip.text: qsTr("Alternative single-panel operator view optimized for DX pile-ups: a tactical 3-column dashboard (Cluster / Waterfall / TX) instead of the classic workspace. Opt-in, default OFF: the standard layout is unchanged when disabled.")
                        }

                        // 1.0.307 (#2) — Scala interfaccia globale (icone+font+layout). Applica al riavvio.
                        Text { text: qsTr("UI Scale:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                        DecoComboBox {
                            id: uiScaleCombo
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            readonly property var scaleValues: [1.0, 1.1, 1.25, 1.5, 1.75]
                            model: ["100%", "110%", "125%", "150%", "175%"]
                            currentIndex: {
                                var f = bridge ? Number(bridge.getSetting("uiScaleFactor", 1.0)) : 1.0
                                var best = 0; var bestd = 99
                                for (var i = 0; i < scaleValues.length; ++i) {
                                    var d = Math.abs(scaleValues[i] - f)
                                    if (d < bestd) { bestd = d; best = i }
                                }
                                return best
                            }
                            onActivated: {
                                if (bridge) bridge.setSetting("uiScaleFactor", scaleValues[currentIndex])
                                uiScaleRestartNote.visible = true
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: parent.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate {
                                contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium }
                            }
                        }
                        Text {
                            id: uiScaleRestartNote
                            Layout.columnSpan: 2
                            Layout.preferredHeight: controlHeight
                            verticalAlignment: Text.AlignVCenter
                            text: qsTr("↻ restart to apply")
                            color: bridge.themeManager.warningColor
                            font.pixelSize: 11
                            visible: false
                        }

                        // ── Bande Operative (#4) — quali bande mostrare nel selettore ──
                        Text { text: qsTr("BANDE OPERATIVE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }
                        Text {
                            text: qsTr("Click to show/hide bands in the selector. Deselected bands disappear from the HF / V-U / SHF bar.")
                            color: textSecondary; font.pixelSize: 10; wrapMode: Text.WordWrap
                            Layout.columnSpan: 4; Layout.fillWidth: true; Layout.bottomMargin: 2
                        }
                        Flow {
                            Layout.columnSpan: 4; Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: settingsDialog.allBandsForConfig
                                delegate: Rectangle {
                                    width: 64; height: 26; radius: 4
                                    property bool bandOn: settingsDialog.bandEnabledCfg(modelData.l)
                                    color: bandOn ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.28)
                                                  : Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b, 0.05)
                                    border.color: bandOn ? primaryBlue : glassBorder
                                    border.width: 1
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.l
                                        color: bandOn ? textPrimary : textSecondary
                                        font.pixelSize: 10; font.bold: bandOn
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        hoverEnabled: true
                                        onClicked: settingsDialog.toggleBandCfg(modelData.l, !parent.bandOn)
                                        ToolTip.visible: containsMouse
                                        ToolTip.delay: 500
                                        ToolTip.text: modelData.n + " MHz — " + (parent.bandOn ? qsTr("visible (click to hide)") : qsTr("hidden (click to show)"))
                                    }
                                }
                            }
                        }

                        // 1.0.189 — Riorganizzato in 2 sub-section per UX migliore:
                        // PERFORMANCE (gates anti-stall) + STYLE (estetica).
                        // ── UI — PERFORMANCE ──
                        Text { text: qsTr("UI — PERFORMANCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        // 1.0.180 — Quality preset: gate per effetti visivi pesanti.
                        Text { text: qsTr("UI Quality preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        DecoComboBox {
                            id: uiQualityCombo
                            Layout.preferredWidth: 180
                            Layout.columnSpan: 1
                            model: ["Low", "Medium", "High"]
                            currentIndex: {
                                if (!bridge) return 1
                                const q = bridge.uiQuality
                                return q === "Low" ? 0 : (q === "High" ? 2 : 1)
                            }
                            onActivated: {
                                if (bridge) bridge.setUiQuality(model[currentIndex])
                            }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Low = no effects (modest PCs).\nMedium = light animations.\nHigh = all available animations.\n\nDefault: Medium.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.388 — Priorità processo Windows (scheduling CPU)
                        Text { text: qsTr("Process priority:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        DecoComboBox {
                            id: processPriorityCombo
                            Layout.preferredWidth: 180
                            Layout.columnSpan: 1
                            model: [qsTr("Normal"), qsTr("Above normal"), qsTr("High (recommended)"), qsTr("Realtime ⚠️")]
                            currentIndex: bridge ? bridge.processPriority : 1
                            onActivated: {
                                if (bridge && bridge.processPriority !== currentIndex)
                                    bridge.processPriority = currentIndex
                            }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("CPU scheduling priority for the Decodium process (Windows).\n\nNormal / Above normal (default) = safe.\nHigh = smoother audio/decode with low risk (recommended if you notice stutters).\nRealtime ⚠️ = maximum scheduling priority, but it can make the PC unresponsive (mouse/keyboard) and requires administrator privileges. Without admin rights Windows downgrades it to High.\n\nIf unsure, use High.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.180 — Style (richiede restart)
                        Text { text: qsTr("UI Style (restart):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        DecoComboBox {
                            id: uiStyleCombo
                            Layout.preferredWidth: 180
                            Layout.columnSpan: 1
                            // 1.0.185 — Whitelist 4 stili customizable. "Default" rimosso
                            // dal model: era un alias confondente perche' su Windows con
                            // Qt 6.11 risolveva al native style non-customizable (warning
                            // massivi + UI degradata). Material e' la prima voce, baseline
                            // visiva storica Decodium (default fino al 1.0.179).
                            model: ["Material", "FluentWinUI3", "Universal", "Fusion"]
                            currentIndex: {
                                if (!bridge) return 0
                                // Default e' alias per Material, mostra Material
                                let idx = model.indexOf(bridge.uiStyle)
                                return idx < 0 ? 0 : idx
                            }
                            onActivated: {
                                if (bridge) bridge.setUiStyle(model[currentIndex])
                            }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr(
                                "QML Quick Controls style (requires restart):\n" +
                                "• Material (recommended) — Google Material 3, customizable, Decodium's historical default\n" +
                                "• FluentWinUI3 — native Windows 11 (Mica/acrylic). Automatic fallback for SplitView/StackView.\n" +
                                "• Universal — Microsoft Universal (WinPhone-style)\n" +
                                "• Fusion — neutral cross-platform desktop"
                            )
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.180 — Frameless pop-out
                        Text { text: qsTr("Frameless pop-out:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        CheckBox {
                            id: framelessPopoutsCheck
                            Layout.leftMargin: 24
                            checked: bridge ? bridge.uiFramelessPopouts : false
                            onCheckedChanged: {
                                if (bridge) bridge.setUiFramelessPopouts(checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Pop-out windows (Waterfall, Period1, DecoSync) become frameless with drag via the border.\n\nWindows 11 aesthetic.\n\nDefault: OFF. Requires closing and reopening the window.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.186 — Auto-detach Full Spectrum (Pasquale-pattern)
                        Text { text: qsTr("Detach Full Spectrum:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        CheckBox {
                            id: autoDetachFullSpectrumCheck
                            Layout.leftMargin: 24
                            checked: bridge ? bridge.autoDetachFullSpectrum : false
                            onCheckedChanged: {
                                if (bridge) bridge.setAutoDetachFullSpectrum(checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("At startup, opens Full Spectrum (Band Activity) in a separate window, isolating the Main render thread from ListView animations.\n\nReduces stalls on modest PCs.\n\nDefault: OFF. Requires restart.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.412 — Schermo intero (opt-in, non persistito: al riavvio torna a finestra)
                        Text { text: qsTr("Full screen:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        Button {
                            Layout.leftMargin: 24
                            Layout.preferredHeight: controlHeight
                            text: qsTr("Enable (F11)")
                            hoverEnabled: true
                            onClicked: { settingsDialog.fullScreenRequested(); settingsDialog.close() }
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Switch Decodium to full screen. To exit: F11, Esc, or the top ✕ button. This is not saved: Decodium starts in normal window mode after restart.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.186 — Spectrum FPS cap (15/20/30)
                        Text { text: qsTr("Spectrum FPS cap:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        DecoComboBox {
                            id: spectrumFpsCombo
                            Layout.preferredWidth: 170
                            model: ["15 fps", "20 fps", "30 fps"]
                            currentIndex: {
                                if (!bridge) return 1
                                const cap = bridge.spectrumFpsCap
                                if (cap <= 15) return 0
                                if (cap >= 30) return 2
                                return 1
                            }
                            onActivated: {
                                if (!bridge) return
                                const map = [15, 20, 30]
                                bridge.setSpectrumFpsCap(map[currentIndex])
                            }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Maximum frame rate of the embedded waterfall/panadapter.\n\n  • 15 = modest PCs\n  • 20 = balanced default\n  • 30 = modern hardware\n\nWhen Full Spectrum is detached the separate render thread holds 30 fps without affecting the decoder.")
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // 1.0.189 — Telemetria pressione CPU (sessione corrente, read-only).
                        // Se i contatori sono alti, considera Low Quality / FPS cap=15 / Detach ON.
                        Text { text: qsTr("CPU pressure:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 140; Layout.columnSpan: 1 }
                        Text {
                            id: cpuPressureTelemetryText
                            Layout.preferredWidth: 280
                            Layout.columnSpan: 1
                            color: {
                                if (!bridge) return textSecondary
                                const severe = bridge.cpuPressureSevereEventCount
                                if (severe >= 5) return "#ff8844"
                                if (severe >= 1) return secondaryCyan
                                return textSecondary
                            }
                            font.pixelSize: 12
                            text: bridge
                                  ? qsTr("events: total=%1 · severe=%2 (session)")
                                        .arg(bridge.cpuPressureEventCount)
                                        .arg(bridge.cpuPressureSevereEventCount)
                                  : qsTr("events: total=0 · severe=0")
                            // 1.0.190 hotfix — hoverEnabled / ToolTip.* non sono
                            // proprieta' di Text. Tooltip e' attached property
                            // gestita da MouseArea con .text dedicato.
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.NoButton
                                ToolTip.visible: containsMouse
                                ToolTip.delay: 400
                                ToolTip.text: qsTr("cpuPressure event counters for the current session.\n\nSevere ones (≥1100ms or burst of 4+ short stalls) are the strongest signal: if you see ≥5 after an hour of use, lower UI Quality to Low or Spectrum FPS cap to 15.")
                            }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Font ──
                        Text { text: qsTr("FONT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlHeight
                                radius: 4
                                color: bgMedium
                                border.color: glassBorder
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: settingsDialog.uiFontLabel
                                    color: textPrimary
                                    font.pixelSize: controlFontSize
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle {
                                width: 78; height: controlHeight; radius: 4
                                color: fontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                                MouseArea { id: fontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.openFontPicker("Font", "", 0, false) }
                            }
                            Rectangle {
                                width: 64; height: controlHeight; radius: 4
                                color: fontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                                MouseArea { id: fontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("Font", "", 0) }
                            }
                        }
                        Text { text: qsTr("Decoded Font:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlHeight
                                radius: 4
                                color: bgMedium
                                border.color: glassBorder
                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    text: settingsDialog.decodedFontLabel
                                    color: textPrimary
                                    font.pixelSize: controlFontSize
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }
                            }
                            Rectangle {
                                width: 78; height: controlHeight; radius: 4
                                color: decodedFontChooseMA.containsMouse ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text { anchors.centerIn: parent; text: qsTr("Choose"); color: primaryBlue; font.pixelSize: 11 }
                                MouseArea { id: decodedFontChooseMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: settingsDialog.openFontPicker("DecodedTextFont", "Courier", 10, true) }
                            }
                            Rectangle {
                                width: 64; height: controlHeight; radius: 4
                                color: decodedFontResetMA.containsMouse ? Qt.rgba(textSecondary.r,textSecondary.g,textSecondary.b,0.18) : bgMedium
                                border.color: glassBorder
                                Text { anchors.centerIn: parent; text: qsTr("Reset"); color: textSecondary; font.pixelSize: 11 }
                                MouseArea { id: decodedFontResetMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: bridge.resetFontSetting("DecodedTextFont", "Courier", 10) }
                            }
                        }

                        // ── Decodifiche ──
                        Text { text: qsTr("DECODES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Show DXCC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowDXCC", true)
                            onCheckedChanged: bridge.setSetting("ShowDXCC", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("US State:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlHeight
                            spacing: 8
                            CheckBox {
                                checked: bridge.showUsState
                                onCheckedChanged: bridge.showUsState = checked
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: controlHeight
                                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                contentItem: Text { text: ""; leftPadding: 24 }
                            }
                            Text {
                                text: bridge.usStateDataUpdating ? qsTr("Updating...")
                                      : (bridge.usStateDataReady ? qsTr("%1 calls").arg(bridge.usStateGridCount)
                                                                 : qsTr("Not loaded"))
                                color: bridge.usStateDataReady ? accentGreen : textSecondary
                                font.pixelSize: 11
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                Layout.preferredHeight: controlHeight
                            }
                            Button {
                                text: qsTr("Update")
                                enabled: bridge.showUsState && !bridge.usStateDataUpdating
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 90
                                onClicked: bridge.updateUsStateData()
                            }
                        }

                        Text { text: qsTr("TX Msg to RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TXMessagesToRX", true)
                            onCheckedChanged: bridge.setSetting("TXMessagesToRX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Waterfall Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiWaterfallShowCallsigns", true)
                            onCheckedChanged: bridge.setSetting("uiWaterfallShowCallsigns", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("FS Dist:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiFullSpectrumShowDistColumn", true)
                            onCheckedChanged: bridge.setSetting("uiFullSpectrumShowDistColumn", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("FS Az:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiFullSpectrumShowAzColumn", true)
                            onCheckedChanged: bridge.setSetting("uiFullSpectrumShowAzColumn", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("RX Freq:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiSignalRxShowFreqColumn", true)
                            onCheckedChanged: bridge.setSetting("uiSignalRxShowFreqColumn", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("RX Dist:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiSignalRxShowDistColumn", true)
                            onCheckedChanged: bridge.setSetting("uiSignalRxShowDistColumn", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("RX Az:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("uiSignalRxShowAzColumn", true)
                            onCheckedChanged: bridge.setSetting("uiSignalRxShowAzColumn", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Mappa e Distanza ──
                        Text { text: qsTr("MAP AND DISTANCE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Miles:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: boolSetting("Miles", false)
                            onCheckedChanged: bridge.setSetting("Miles", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Greyline:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ShowGreyline", false)
                            onCheckedChanged: bridge.setSetting("ShowGreyline", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Map All Msgs:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapAllMessages", false)
                            onCheckedChanged: bridge.setSetting("MapAllMessages", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Click TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("MapSingleClickTX", false)
                            onCheckedChanged: bridge.setSetting("MapSingleClickTX", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Allineamento ──
                        Text { text: qsTr("ALIGNMENT"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Align:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Align", false)
                            onCheckedChanged: bridge.setSetting("Align", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Align Steps:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignStepsSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps", value)
                            contentItem: TextInput { text: alignStepsSpin.textFromValue(alignStepsSpin.value, alignStepsSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !alignStepsSpin.editable; validator: alignStepsSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Align Steps 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: alignSteps2Spin
                            from: 0; to: 999; value: Number(bridge.getSetting("AlignSteps2", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AlignSteps2", value)
                            contentItem: TextInput { text: alignSteps2Spin.textFromValue(alignSteps2Spin.value, alignSteps2Spin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !alignSteps2Spin.editable; validator: alignSteps2Spin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 4; Layout.preferredHeight: 18 }
                    }
                }

                // ═══════════ TAB 5 — DECODIFICA ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Remote Web Server (PWA per iPad/mobile) ──
                        Text { text: qsTr("REMOTE WEB SERVER (iPad / mobile PWA)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Abilita Web Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
                        CheckBox {
                            id: webServerToggle
                            checked: bridge.webServerRunning()
                            onCheckedChanged: {
                                if (checked) {
                                    var port = parseInt(webServerPortField.text) || 8080
                                    bridge.startWebServer(port)
                                    bridge.setSetting("WebServerEnabled", true)
                                    bridge.setSetting("WebServerPort", port)
                                } else {
                                    bridge.stopWebServer()
                                    bridge.setSetting("WebServerEnabled", false)
                                }
                                webServerUrlLabel.text = bridge.webServerUrl() || qsTr("(not active)")
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Porta TCP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
                        DecoTextField {
                            id: webServerPortField
                            text: String(bridge.getSetting("WebServerPort", 8080))
                            Layout.preferredWidth: 80
                            validator: IntValidator { bottom: 1024; top: 65535 }
                            color: textPrimary
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 3 }
                            onEditingFinished: bridge.setSetting("WebServerPort", parseInt(text) || 8080)
                        }

                        Text { text: qsTr("URL accesso:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
                        Text {
                            id: webServerUrlLabel
                            text: bridge.webServerUrl() || qsTr("(not active)")
                            color: bridge.webServerRunning() ? accentGreen : textSecondary
                            font.pixelSize: 12
                            font.family: decodiumMonoFontFamily
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                        }

                        Text { text: qsTr(""); Layout.preferredWidth: 160 }
                        Button {
                            // PWA remote: apre l'URL locale autenticato in browser.
                            text: qsTr("📱 Open Remote for iPad")
                            enabled: bridge.webServerRunning()
                            Layout.columnSpan: 3
                            onClicked: {
                                var url = bridge.webServerQrUrl()
                                if (url) Qt.openUrlExternally(url)
                            }
                            background: Rectangle {
                                color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.3)
                                                     : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.15)
                                border.color: primaryBlue
                                border.width: 1
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? textPrimary : textSecondary
                                font.pixelSize: 12; font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // ── Decode list display (Decodium 3-style) ──
                        Text { text: qsTr("DECODE LIST DISPLAY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 12 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Colored period separator:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
                        CheckBox {
                            // 1.0.149: bind diretto al Q_INVOKABLE C++ invece che
                            // alla QSettings raw — cosi' il toggle aggiorna anche
                            // m_decodeShowPeriodSeparator a runtime (era solo
                            // letto al boot via loadSettings).
                            checked: bridge.decodeShowPeriodSeparator()
                            onCheckedChanged: {
                                bridge.setDecodeShowPeriodSeparator(checked)
                                bridge.setSetting("decodeShowPeriodSeparator", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Newest first:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 160 }
                        CheckBox {
                            checked: bridge.getSetting("decodeNewestFirst", false)
                            onCheckedChanged: bridge.setSetting("decodeNewestFirst", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Parametri Decodifica ──
                        Text { text: qsTr("DECODE PARAMETERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Decode Depth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: decodeDepthCombo
                            model: [qsTr("Fast"),qsTr("Normal"),qsTr("Deep")]; Layout.fillWidth: true; Layout.minimumWidth: comboFieldMinWidth; Layout.preferredWidth: comboFieldMinWidth; implicitHeight: controlHeight
                            currentIndex: Math.max(0, Math.min(count - 1, bridge.ndepth - 1))
                            onActivated: {
                                bridge.ndepth = currentIndex + 1
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: decodeDepthCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; rightPadding: 42; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Low Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: nfaSpin
                            from: 0; to: 5000; value: bridge.nfa; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: {
                                bridge.nfa = value
                                settingsDialog.scheduleSettingsPersist()
                            }
                            contentItem: TextInput { text: nfaSpin.textFromValue(nfaSpin.value, nfaSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !nfaSpin.editable; validator: nfaSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("High Freq (Hz):"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: nfbSpin
                            from: 0; to: 5000; value: bridge.nfb; editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: {
                                bridge.nfb = value
                                settingsDialog.scheduleSettingsPersist()
                            }
                            contentItem: TextInput { text: nfbSpin.textFromValue(nfbSpin.value, nfbSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !nfbSpin.editable; validator: nfbSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("RX Bandwidth:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: rxBwSpin
                            from: 100; to: 5000; value: Number(bridge.getSetting("RXBandwidth", 2500)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.minimumWidth: numericFieldMinWidth; Layout.preferredWidth: numericFieldMinWidth
                            onValueChanged: bridge.setSetting("RXBandwidth", value)
                            contentItem: TextInput { text: rxBwSpin.textFromValue(rxBwSpin.value, rxBwSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !rxBwSpin.editable; validator: rxBwSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Decode at 52s:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            checked: bridge.getSetting("DecodeAt52s", false)
                            onCheckedChanged: bridge.setSetting("DecodeAt52s", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Single Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            checked: bridge.singleDecode
                            onToggled: {
                                bridge.singleDecode = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── JT65 VHF/UHF ──
                        Text { text: qsTr("JT65 VHF/UHF"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Erasure Patterns:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: erasurePatSpin
                            from: 0; to: 99999; value: Number(bridge.getSetting("RandomErasurePatterns", 7)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("RandomErasurePatterns", value)
                            contentItem: TextInput { text: erasurePatSpin.textFromValue(erasurePatSpin.value, erasurePatSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !erasurePatSpin.editable; validator: erasurePatSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Aggressive:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: aggressiveSpin
                            from: 0; to: 10; value: Number(bridge.getSetting("AggressiveLevel", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("AggressiveLevel", value)
                            contentItem: TextInput { text: aggressiveSpin.textFromValue(aggressiveSpin.value, aggressiveSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !aggressiveSpin.editable; validator: aggressiveSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Two-Pass:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("TwoPassDecoding", false)
                            onCheckedChanged: bridge.setSetting("TwoPassDecoding", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Sidelobe Control ──
                        Text { text: qsTr("SIDELOBE CONTROL"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Sidelobe Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: sidelobeCombo
                            model: [qsTr("Low Sidelobes"),qsTr("Max Sensitivity")]; Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: Number(bridge.getSetting("SidelobeMode", 0))
                            onActivated: bridge.setSetting("SidelobeMode", currentIndex)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: sidelobeCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Degrade S/N:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: degradeSnSpin
                            from: 0; to: 100; value: Number(bridge.getSetting("DegradeSN", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("DegradeSN", value)
                            contentItem: TextInput { text: degradeSnSpin.textFromValue(degradeSnSpin.value, degradeSnSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !degradeSnSpin.editable; validator: degradeSnSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Filtri Decodifica ──
                        Text { text: qsTr("DECODE FILTERS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("CQ Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterCqOnly
                            onCheckedChanged: {
                                bridge.filterCqOnly = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        // 1.0.383 — livello di inclusione del filtro CQ (attivo solo con "CQ Only" ON).
                        Text { text: qsTr("CQ filter:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: cqFilterLevelCombo
                            enabled: bridge.filterCqOnly
                            opacity: enabled ? 1.0 : 0.5
                            model: ["CQ", "CQ/73", "CQ/73/RR73", "CQ/73/RR73/RRR"]
                            Layout.fillWidth: true; implicitHeight: controlHeight
                            currentIndex: bridge ? bridge.cqFilterLevel : 0
                            onActivated: {
                                if (bridge && bridge.cqFilterLevel !== currentIndex) {
                                    bridge.cqFilterLevel = currentIndex
                                    settingsDialog.scheduleSettingsPersist()
                                }
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: cqFilterLevelCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup: SettingsComboPopup { combo: cqFilterLevelCombo }
                        }
                        Text { text: qsTr("My Call Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.filterMyCallOnly
                            onCheckedChanged: {
                                bridge.filterMyCallOnly = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Zap:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.zapEnabled
                            onCheckedChanged: {
                                bridge.zapEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Deep Search:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.deepSearchEnabled
                            onCheckedChanged: {
                                bridge.deepSearchEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("AP Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.ft8ApEnabled
                            onCheckedChanged: {
                                bridge.ft8ApEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Avg Decode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.avgDecodeEnabled
                            onCheckedChanged: {
                                bridge.avgDecodeEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // 1.0.299 — deep decode-list-only durante TX (recupera stazioni terze in QSO)
                        Text { text: qsTr("Deep decode in TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.ft8DeepDecodeInTx
                            onCheckedChanged: {
                                bridge.ft8DeepDecodeInTx = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("While operating/in QSO in FT8, ALSO launch the deep decode depth-4 (decode-list only) in addition to the fast depth-2 that drives TX.\n\nRecovers third-party stations that the fast pass would miss during operation, WITHOUT touching timing or QSO closure (it's pure decode-list, not auto-seq).\n\nCosts extra CPU during QSOs. Default: OFF.")
                        }
                    }
                }

                // ═══════════ TAB 6 — REPORTING ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Servizi di Rete ──
                        Text { text: qsTr("NETWORK SERVICES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("PSK Reporter:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.pskReporterEnabled
                            onCheckedChanged: {
                                bridge.pskReporterEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("TCP/IP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("PSKReporterTCPIP", false)
                            onCheckedChanged: bridge.setSetting("PSKReporterTCPIP", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── DX Cluster ──
                        Text { text: qsTr("DX CLUSTER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            id: dxClusterHostField
                            text: bridge.dxCluster && bridge.dxCluster.host !== undefined ? String(bridge.dxCluster.host) : ""
                            Layout.fillWidth: true
                            Layout.minimumWidth: wideFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: "dx.iz7auh.net"
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: if (bridge.dxCluster) bridge.dxCluster.host = text.trim()
                        }
                        Text { text: qsTr("Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: dxClusterPortSpin
                            from: 1; to: 65535
                            value: {
                                var port = bridge.dxCluster && bridge.dxCluster.port !== undefined ? Number(bridge.dxCluster.port) : 8000
                                return isFinite(port) ? port : 8000
                            }
                            editable: true
                            implicitHeight: controlHeight
                            Layout.fillWidth: true
                            Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: if (bridge.dxCluster) bridge.dxCluster.port = value
                            contentItem: TextInput { text: dxClusterPortSpin.textFromValue(dxClusterPortSpin.value, dxClusterPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !dxClusterPortSpin.editable; validator: dxClusterPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 10

                            Text {
                                text: bridge.dxCluster && bridge.dxCluster.connected ? qsTr("Connected") : qsTr("Disconnected")
                                color: bridge.dxCluster && bridge.dxCluster.connected ? accentGreen : textSecondary
                                font.pixelSize: 12
                            }

                            Rectangle {
                                width: 96; height: controlHeight; radius: 4
                                color: dxClusterConnMA.containsMouse ? Qt.rgba(accentGreen.r, accentGreen.g, accentGreen.b, 0.25) : bgMedium
                                border.color: accentGreen
                                Text { anchors.centerIn: parent; text: qsTr("Connect"); color: accentGreen; font.pixelSize: 12 }
                                MouseArea {
                                    id: dxClusterConnMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (!bridge.dxCluster) return
                                        bridge.dxCluster.host = dxClusterHostField.text.trim()
                                        bridge.dxCluster.port = dxClusterPortSpin.value
                                        bridge.dxCluster.callsign = bridge.callsign
                                        bridge.connectDxCluster()
                                    }
                                }
                            }

                            Rectangle {
                                width: 110; height: controlHeight; radius: 4
                                color: dxClusterDiscMA.containsMouse ? Qt.rgba(0.95,0.26,0.21,0.2) : bgMedium
                                border.color: "#f44336"
                                Text { anchors.centerIn: parent; text: qsTr("Disconnect"); color: "#f44336"; font.pixelSize: 12 }
                                MouseArea {
                                    id: dxClusterDiscMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: bridge.disconnectDxCluster()
                                }
                            }
                        }

                        Text { text: qsTr("Detail:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        Text {
                            text: bridge.dxCluster && bridge.dxCluster.lastStatus ? bridge.dxCluster.lastStatus : qsTr("No message")
                            color: textSecondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                        }

                        // ── Cloudlog ──
                        Text { text: qsTr("CLOUDLOG"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.cloudlogEnabled
                            onCheckedChanged: {
                                bridge.cloudlogEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("API URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.cloudlogUrl; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.cloudlogUrl = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }

                        Text { text: qsTr("API Key:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.cloudlogApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
                                bridge.cloudlogApiKey = text
                                settingsDialog.scheduleSettingsPersist()
                            }
                        }

                        Text { text: qsTr("Station ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: cloudlogStIdSpin
                            from: 0; to: 999; value: Number(bridge.getSetting("CloudlogStationID", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("CloudlogStationID", value)
                            contentItem: TextInput { text: cloudlogStIdSpin.textFromValue(cloudlogStIdSpin.value, cloudlogStIdSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !cloudlogStIdSpin.editable; validator: cloudlogStIdSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── QRZ Logbook ──
                        Text { text: qsTr("QRZ LOGBOOK"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.qrzLogbookEnabled
                            onCheckedChanged: {
                                bridge.qrzLogbookEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Replace duplicates:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 130 }
                        CheckBox {
                            checked: bridge.qrzLogbookReplaceDuplicates
                            onCheckedChanged: {
                                bridge.qrzLogbookReplaceDuplicates = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("API Key:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.qrzLogbookApiKey; Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; Layout.columnSpan: 3
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: {
	                                bridge.qrzLogbookApiKey = text
	                                settingsDialog.scheduleSettingsPersist()
	                                settingsDialog.qrzLogbookTestStatus = ""
                                settingsDialog.qrzLogbookTestIsError = false
                                settingsDialog.qrzLogbookTestBusy = false
                            }
                        }

                        Text { text: qsTr("Status:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Rectangle {
                            width: 110; height: controlHeight; radius: 4
                            opacity: settingsDialog.qrzLogbookTestBusy ? 0.75 : 1
                            color: qrzTestMA.containsMouse && !settingsDialog.qrzLogbookTestBusy ? Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.2) : bgMedium
                            border.color: settingsDialog.qrzLogbookTestBusy ? textSecondary : secondaryCyan
                            Text {
                                anchors.centerIn: parent
                                text: settingsDialog.qrzLogbookTestBusy ? qsTr("Testing...") : qsTr("Test")
                                color: settingsDialog.qrzLogbookTestBusy ? textSecondary : secondaryCyan
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: qrzTestMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: !settingsDialog.qrzLogbookTestBusy
                                onClicked: {
                                    settingsDialog.qrzLogbookTestBusy = true
                                    settingsDialog.qrzLogbookTestIsError = false
                                    settingsDialog.qrzLogbookTestStatus = qsTr("Testing QRZ API key...")
                                    bridge.testQrzLogbookApi()
                                }
                            }
                        }
                        Text {
                            text: settingsDialog.qrzLogbookTestStatus
                            visible: text.length > 0
                            color: settingsDialog.qrzLogbookTestIsError ? "#ff5252" : (settingsDialog.qrzLogbookTestBusy ? textSecondary : accentGreen)
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            Layout.preferredHeight: Math.max(controlHeight, implicitHeight)
                        }

                        // ── LotW ──
                        Text { text: qsTr("LOTW"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("LotW Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.lotwEnabled
                            onCheckedChanged: {
                                bridge.lotwEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Password:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("LoTWPassword", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("LoTWPassword", text)
                        }

                        Text { text: qsTr("Non-QSL'd:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LoTWNonQSL", false)
                            onCheckedChanged: bridge.setSetting("LoTWNonQSL", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Days Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: lotwDaysSpin
                            from: 0; to: 9999; value: Number(bridge.getSetting("LoTWDaysSinceUpload", 365)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("LoTWDaysSinceUpload", value)
                            contentItem: TextInput { text: lotwDaysSpin.textFromValue(lotwDaysSpin.value, lotwDaysSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !lotwDaysSpin.editable; validator: lotwDaysSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        // ── Logging ──
                        Text { text: qsTr("LOGGING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Prompt to Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: promptToLogCheck
                            checked: boolSetting("PromptToLog", false)
                            onToggled: {
                                if (!settingsDialog.loggingChecksUpdating)
                                    settingsDialog.setLoggingMode(checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Auto Log:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: autoLogCheck
                            checked: boolSetting("AutoLog", true)
                            onToggled: {
                                if (!settingsDialog.loggingChecksUpdating)
                                    settingsDialog.setLoggingMode(!checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            Component.onCompleted: Qt.callLater(function() { settingsDialog.normalizeLoggingModeChecks() })
                        }

                        Text { text: qsTr("Log as RTTY:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("LogAsRTTY", false)
                            onCheckedChanged: bridge.setSetting("LogAsRTTY", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("4-digit Grids:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Log4DigitGrids", false)
                            onCheckedChanged: bridge.setSetting("Log4DigitGrids", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Contest Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            enabled: !promptToLogCheck.checked
                            opacity: enabled ? 1.0 : 0.45
                            checked: bridge.getSetting("ContestingOnly", false)
                            onCheckedChanged: bridge.setSetting("ContestingOnly", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Spec Op Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("SpecOpInComments", false)
                            onCheckedChanged: bridge.setSetting("SpecOpInComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("dB in Cmts:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("dBReportsToComments", false)
                            onCheckedChanged: bridge.setSetting("dBReportsToComments", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("ZZ00:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("ZZ00", false)
                            onCheckedChanged: bridge.setSetting("ZZ00", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Registrazione ──
                        Text { text: qsTr("RECORDING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Record RX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordRxEnabled
                            onCheckedChanged: {
                                bridge.recordRxEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Record TX:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.recordTxEnabled
                            onCheckedChanged: {
                                bridge.recordTxEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("WSPR Upload:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.wsprUploadEnabled
                            onCheckedChanged: {
                                bridge.wsprUploadEnabled = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── Remote Web Dashboard ──
                        Text { text: qsTr("REMOTE WEB DASHBOARD (LAN)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("RemoteWebEnabled", false)
                            onCheckedChanged: bridge.setSetting("RemoteWebEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("HTTP port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: remoteHttpPortSpin
                            from: 1025; to: 65535; value: Number(bridge.getSetting("RemoteHttpPort", 19091)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("RemoteHttpPort", value)
                            contentItem: TextInput { text: remoteHttpPortSpin.textFromValue(remoteHttpPortSpin.value, remoteHttpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !remoteHttpPortSpin.editable; validator: remoteHttpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("WS socket port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            readOnly: true
                            text: String(bridge.remoteWebSocketPort())
                            Layout.fillWidth: true
                            Layout.preferredWidth: portFieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("WS bind:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("RemoteWsBind", "0.0.0.0"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteWsBind", text)
                        }
                        Text { text: qsTr("Username:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("RemoteUser", "admin"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteUser", text)
                        }

                        Text { text: qsTr("Access token:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("RemoteToken", ""); Layout.fillWidth: true; Layout.columnSpan: 3; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            placeholderText: qsTr("Required for LAN/WAN")
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RemoteToken", text)
                        }

                        Text {
                            text: qsTr("App restart required. For LAN/WAN, use a token of at least 12 characters.")
                            color: textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.columnSpan: 4
                        }

                        // ── UDP Server ──
                        Text { text: qsTr("UDP SERVER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Client ID:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            id: udpClientIdField
                            text: bridge.getSetting("UDPClientId", "WSJTX")
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            leftPadding: 8
                            maximumLength: 64
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            inputMethodHints: Qt.ImhNoPredictiveText
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: {
                                var cleaned = String(text).trim()
                                if (!cleaned.length)
                                    cleaned = "WSJTX"
                                if (cleaned !== text)
                                    text = cleaned
                                bridge.setSetting("UDPClientId", cleaned)
                            }
                        }
                        Text { text: qsTr("Preset:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: udpClientIdPreset
                            model: ["WSJTX", "Decodium"]
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: currentIndex = Math.max(0, find(String(bridge.getSetting("UDPClientId", "WSJTX"))))
                            onActivated: {
                                udpClientIdField.text = currentText
                                bridge.setSetting("UDPClientId", currentText)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpClientIdPreset.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Server Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("UDPServer", "127.0.0.1"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPServer", text)
                        }
                        Text { text: qsTr("Server Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPServerPort", 2237)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPServerPort", value)
                            contentItem: TextInput { text: udpPortSpin.textFromValue(udpPortSpin.value, udpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpPortSpin.editable; validator: udpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Listen Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpListenSpin
                            from: 0; to: 65535; value: Number(bridge.getSetting("UDPListenPort", 0)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPListenPort", value)
                            contentItem: TextInput { text: udpListenSpin.textFromValue(udpListenSpin.value, udpListenSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpListenSpin.editable; validator: udpListenSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Multicast TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPTTL", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPTTL", value)
                            contentItem: TextInput { text: udpTtlSpin.textFromValue(udpTtlSpin.value, udpTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTtlSpin.editable; validator: udpTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Interface Used:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: udpInterfaceCombo
                            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: {
                                var saved = bridge.udpInterfaceName()
                                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
                            }
                            onActivated: bridge.setUdpInterfaceName(currentIndex <= 0 ? "" : currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Send ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: udpPrimaryAdifCheck
                            checked: boolSetting("UDPPrimaryLoggedAdifEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPPrimaryLoggedAdifEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Secondary UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: udpSecondaryCheck
                            checked: boolSetting("UDPSecondaryEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPSecondaryEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Secondary Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("UDPSecondaryServer", bridge.getSetting("UDPServer", "127.0.0.1")); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPSecondaryServer", text)
                        }

                        Text { text: qsTr("Secondary Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpSecondaryPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPSecondaryServerPort", 2239)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPSecondaryServerPort", value)
                            contentItem: TextInput { text: udpSecondaryPortSpin.textFromValue(udpSecondaryPortSpin.value, udpSecondaryPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpSecondaryPortSpin.editable; validator: udpSecondaryPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Secondary TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpSecondaryTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPSecondaryTTL", bridge.getSetting("UDPTTL", 1))); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPSecondaryTTL", value)
                            contentItem: TextInput { text: udpSecondaryTtlSpin.textFromValue(udpSecondaryTtlSpin.value, udpSecondaryTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpSecondaryTtlSpin.editable; validator: udpSecondaryTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Secondary Interface:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: udpSecondaryInterfaceCombo
                            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: {
                                var saved = String(bridge.getSetting("UDPSecondaryInterface", ""))
                                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
                            }
                            onActivated: bridge.setSetting("UDPSecondaryInterface", currentIndex <= 0 ? "" : currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpSecondaryInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Secondary ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: udpSecondaryAdifCheck
                            checked: boolSetting("UDPSecondaryLoggedAdifEnabled", true)
                            onToggled: setBoolSettingIfChanged("UDPSecondaryLoggedAdifEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Tertiary UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: udpTertiaryCheck
                            checked: boolSetting("UDPTertiaryEnabled", false)
                            onToggled: setBoolSettingIfChanged("UDPTertiaryEnabled", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Tertiary Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("UDPTertiaryServer", "127.0.0.1"); Layout.fillWidth: true; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            enabled: udpTertiaryCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("UDPTertiaryServer", text)
                        }

                        Text { text: qsTr("Tertiary Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpTertiaryPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("UDPTertiaryServerPort", 2237)); editable: true
                            enabled: udpTertiaryCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPTertiaryServerPort", value)
                            contentItem: TextInput { text: udpTertiaryPortSpin.textFromValue(udpTertiaryPortSpin.value, udpTertiaryPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTertiaryPortSpin.editable; validator: udpTertiaryPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: udpTertiaryPortSpin.enabled }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("Tertiary TTL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: udpTertiaryTtlSpin
                            from: 0; to: 255; value: Number(bridge.getSetting("UDPTertiaryTTL", bridge.getSetting("UDPTTL", 1))); editable: true
                            enabled: udpTertiaryCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("UDPTertiaryTTL", value)
                            contentItem: TextInput { text: udpTertiaryTtlSpin.textFromValue(udpTertiaryTtlSpin.value, udpTertiaryTtlSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !udpTertiaryTtlSpin.editable; validator: udpTertiaryTtlSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: udpTertiaryTtlSpin.enabled }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Tertiary Interface:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoComboBox {
                            id: udpTertiaryInterfaceCombo
                            model: [qsTr("All interfaces")].concat(bridge.networkInterfaceNames())
                            enabled: udpTertiaryCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            Layout.fillWidth: true
                            Layout.minimumWidth: fieldMinWidth
                            implicitHeight: controlHeight
                            Component.onCompleted: {
                                var saved = String(bridge.getSetting("UDPTertiaryInterface", ""))
                                currentIndex = saved && saved.length ? Math.max(0, find(saved)) : 0
                            }
                            onActivated: bridge.setSetting("UDPTertiaryInterface", currentIndex <= 0 ? "" : currentText)
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: udpTertiaryInterfaceCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12; elide: Text.ElideRight }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Tertiary ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            id: udpTertiaryAdifCheck
                            checked: boolSetting("UDPTertiaryLoggedAdifEnabled", true)
                            enabled: udpTertiaryCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            onToggled: setBoolSettingIfChanged("UDPTertiaryLoggedAdifEnabled", checked, true)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── N1MM Logger+ / EasyLog (binary UDP) ──
                        Text { text: qsTr("N1MM / EasyLog"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enable N1MM:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: n1mmEnableCheck
                            checked: boolSetting("BroadcastToN1MM", false)
                            onToggled: setBoolSettingIfChanged("BroadcastToN1MM", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("N1MM Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: n1mmPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("N1MMServerPort", 2333)); editable: true
                            enabled: n1mmEnableCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("N1MMServerPort", value)
                            contentItem: TextInput { text: n1mmPortSpin.textFromValue(n1mmPortSpin.value, n1mmPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !n1mmPortSpin.editable; validator: n1mmPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: n1mmPortSpin.enabled }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("N1MM Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("N1MMServer", "127.0.0.1"); Layout.fillWidth: true; Layout.columnSpan: 3; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            enabled: n1mmEnableCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("N1MMServer", text)
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 4; Layout.preferredHeight: 6 }

                        Text { text: qsTr("Accept UDP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            // Default allineato con Configuration.cpp (true) per evitare
                            // che il primo onCheckedChanged scriva `false` nel legacy INI
                            // prima che Configuration abbia fatto write_settings.
                            checked: bridge.getSetting("AcceptUDPRequests", true)
                            onCheckedChanged: bridge.setSetting("AcceptUDPRequests", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Notify Request:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("NotifyOnRequest", false)
                            onCheckedChanged: bridge.setSetting("NotifyOnRequest", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Restore Win:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("udpWindowRestore", false)
                            onCheckedChanged: bridge.setSetting("udpWindowRestore", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── ADIF TCP ──
                        Text { text: qsTr("ADIF TCP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enable TCP ADIF:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        CheckBox {
                            id: adifTcpCheck
                            checked: boolSetting("ADIFTcpEnabled", false)
                            onToggled: setBoolSettingIfChanged("ADIFTcpEnabled", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("TCP Port:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        SpinBox {
                            id: adifTcpPortSpin
                            from: 1; to: 65535; value: Number(bridge.getSetting("ADIFTcpPort", 52001)); editable: true
                            enabled: adifTcpCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            implicitHeight: controlHeight; Layout.fillWidth: true; Layout.preferredWidth: portFieldMinWidth
                            onValueChanged: bridge.setSetting("ADIFTcpPort", value)
                            contentItem: TextInput { text: adifTcpPortSpin.textFromValue(adifTcpPortSpin.value, adifTcpPortSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !adifTcpPortSpin.editable; validator: adifTcpPortSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly; enabled: adifTcpPortSpin.enabled }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("TCP Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: labelWidth }
                        DecoTextField {
                            text: bridge.getSetting("ADIFTcpServer", "127.0.0.1"); Layout.fillWidth: true; Layout.columnSpan: 3; Layout.minimumWidth: fieldMinWidth; implicitHeight: controlHeight; leftPadding: 8
                            enabled: adifTcpCheck.checked
                            opacity: enabled ? 1.0 : 0.5
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("ADIFTcpServer", text)
                        }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            Layout.preferredHeight: 96
                        }
                    }
                }

                // ═══════════ TAB 7 — FREQUENCIES ═══════════
                ScrollView {
                    id: frequenciesScrollView
                    clip: true
                    readonly property int pageContentWidth: settingsDialog.frequencyPageMinWidth
                    contentWidth: settingsDialog.frequencyPageMinWidth + settingsDialog.scrollLeftMargin + settingsDialog.scrollRightMargin
                    contentHeight: frequenciesContent.implicitHeight + 20
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        id: frequenciesContent
                        width: settingsDialog.frequencyPageMinWidth
                        anchors { left: parent.left; top: parent.top; margins: 10 }
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: qsTr("FREQUENCY CALIBRATION")
                                color: secondaryCyan
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            Button {
                                id: refreshFrequencyButton
                                text: qsTr("Refresh")
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 94
                                onClicked: settingsDialog.refreshFrequencySettings()
                                background: Rectangle {
                                    color: refreshFrequencyButton.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24) : bgMedium
                                    border.color: glassBorder
                                    radius: 4
                                }
                                contentItem: Text {
                                    text: refreshFrequencyButton.text
                                    color: textSecondary
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 6
                            columnSpacing: 10
                            rowSpacing: 8

                            Text { text: qsTr("Slope:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 80; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                            DecoTextField {
                                id: frequencySlopeField
                                text: Number(bridge.frequencyCalibrationSlopePpm()).toFixed(5)
                                Layout.preferredWidth: 130
                                implicitHeight: controlHeight
                                leftPadding: 8
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                horizontalAlignment: TextInput.AlignRight
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                validator: DoubleValidator { bottom: -200; top: 200; decimals: 5; notation: DoubleValidator.StandardNotation }
                                topPadding: controlVerticalPadding
                                bottomPadding: controlVerticalPadding
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                onEditingFinished: text = settingsDialog.commitFrequencySlope(text)
                            }
                            Text { text: qsTr("ppm"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 44; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }

                            Text { text: qsTr("Intercept:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 88; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                            DecoTextField {
                                id: frequencyInterceptField
                                text: Number(bridge.frequencyCalibrationInterceptHz()).toFixed(2)
                                Layout.preferredWidth: 130
                                implicitHeight: controlHeight
                                leftPadding: 8
                                color: textPrimary
                                font.pixelSize: controlFontSize
                                horizontalAlignment: TextInput.AlignRight
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                validator: DoubleValidator { bottom: -1000; top: 1000; decimals: 2; notation: DoubleValidator.StandardNotation }
                                topPadding: controlVerticalPadding
                                bottomPadding: controlVerticalPadding
                                verticalAlignment: TextInput.AlignVCenter
                                background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                onEditingFinished: text = settingsDialog.commitFrequencyIntercept(text)
                            }
                            Text { text: qsTr("Hz"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 22; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                            // 1.0.192 — Reset button per Frequency Calibration (slope=0, intercept=0)
                            Button {
                                id: frequencyCalibrationResetButton
                                text: qsTr("Reset")
                                implicitHeight: controlHeight
                                Layout.fillWidth: true
                                onClicked: {
                                    if (!bridge) return
                                    bridge.setFrequencyCalibrationSlopePpm(0.0)
                                    bridge.setFrequencyCalibrationInterceptHz(0.0)
                                    frequencySlopeField.text = Number(0).toFixed(5)
                                    frequencyInterceptField.text = Number(0).toFixed(2)
                                }
                                background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: parent.text; color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                hoverEnabled: true
                                ToolTip.visible: hovered
                                ToolTip.delay: 400
                                ToolTip.text: qsTr("Reset calibration (slope=0, intercept=0). The frequency is written to the rig without correction (fast path).")
                            }
                        }

                        // 1.0.193 — Live preview della correzione su frequenze tipiche FT8
                        // (banda 20m 14.074 MHz, banda 10m 28.074 MHz). Aggiornato a ogni
                        // edit dei TextField slope/intercept tramite property binding.
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            spacing: 12
                            Text {
                                text: qsTr("Preview correzione:")
                                color: textSecondary
                                font.pixelSize: 11
                                font.italic: true
                            }
                            Text {
                                id: frequencyCalibrationPreview
                                readonly property double slope: parseFloat(frequencySlopeField.text) || 0.0
                                readonly property double intercept: parseFloat(frequencyInterceptField.text) || 0.0
                                readonly property double delta14: 14074000.0 * slope * 1e-6 + intercept
                                readonly property double delta28: 28074000.0 * slope * 1e-6 + intercept
                                text: qsTr("14.074 MHz → %1 Hz · 28.074 MHz → %2 Hz")
                                          .arg(delta14.toFixed(2))
                                          .arg(delta28.toFixed(2))
                                color: (Math.abs(delta14) > 50 || Math.abs(delta28) > 100)
                                       ? "#ff8844" : secondaryCyan
                                font.pixelSize: 11
                                font.family: mainWindow.decodedTextFontFamily
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            Text {
                                text: qsTr("WORKING FREQUENCIES")
                                color: secondaryCyan
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            Button {
                                id: loadWorkingFrequenciesButton
                                text: qsTr("Load")
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 78
                                onClicked: settingsDialog.openWorkingFrequenciesLoadDialog(false)
                                background: Rectangle { color: loadWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: loadWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                            Button {
                                id: mergeWorkingFrequenciesButton
                                text: qsTr("Merge")
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 78
                                onClicked: settingsDialog.openWorkingFrequenciesLoadDialog(true)
                                background: Rectangle { color: mergeWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: mergeWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                            Button {
                                id: saveWorkingFrequenciesButton
                                text: qsTr("Save as")
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 88
                                onClicked: settingsDialog.openWorkingFrequenciesSaveDialog()
                                background: Rectangle { color: saveWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: saveWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                            Button {
                                id: resetWorkingFrequenciesButton
                                text: qsTr("Defaults")
                                implicitHeight: controlHeight
                                Layout.preferredWidth: 104
                                onClicked: {
                                    bridge.resetWorkingFrequenciesToDefaults()
                                    settingsDialog.clearWorkingFrequencyEditor()
                                    settingsDialog.refreshFrequencySettings()
                                }
                                background: Rectangle { color: resetWorkingFrequenciesButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: glassBorder; radius: 4 }
                                contentItem: Text { text: resetWorkingFrequenciesButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 168
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.44)
                            border.color: glassBorder
                            radius: 6
                            clip: true

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text { text: qsTr("Region:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 58; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoComboBox {
                                        id: frequencyRegionCombo
                                        model: settingsDialog.frequencyRegionOptions
                                        Layout.preferredWidth: 132
                                        implicitHeight: controlHeight
                                    }
                                    Text { text: qsTr("Mode:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 44; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoComboBox {
                                        id: frequencyModeCombo
                                        model: settingsDialog.frequencyModeOptions
                                        Layout.preferredWidth: 124
                                        implicitHeight: controlHeight
                                        Component.onCompleted: settingsDialog.setComboText(frequencyModeCombo, bridge.mode || "FT8")
                                    }
                                    Text { text: qsTr("Freq MHz:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 68; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: frequencyMHzField
                                        placeholderText: "14.074000"
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        horizontalAlignment: TextInput.AlignRight
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 120
                                        implicitHeight: controlHeight
                                        background: Rectangle {
                                            color: bgMedium
                                            border.color: parent.activeFocus ? secondaryCyan
                                                                               : (parent.text.length > 0 && !settingsDialog.workingFrequencyEditorHasValidFrequency() ? "#ff7777" : glassBorder)
                                            radius: 4
                                        }
                                    }
                                    CheckBox {
                                        id: frequencyPreferredCheck
                                        text: qsTr("Pref")
                                        Layout.preferredWidth: 82
                                        implicitHeight: controlHeight
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text { text: qsTr("Description:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 94; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: frequencyDescriptionField
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        Layout.fillWidth: true
                                        implicitHeight: controlHeight
                                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text { text: qsTr("Start:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 46; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: frequencyStartField
                                        placeholderText: "yyyy-MM-dd HH:mm"
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 170
                                        implicitHeight: controlHeight
                                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                    }
                                    Text { text: qsTr("End:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 38; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: frequencyEndField
                                        placeholderText: "yyyy-MM-dd HH:mm"
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 170
                                        implicitHeight: controlHeight
                                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        id: addWorkingFrequencyButton
                                        text: qsTr("Add")
                                        enabled: settingsDialog.workingFrequencyEditorHasValidFrequency()
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 86
                                        onClicked: settingsDialog.addWorkingFrequencyFromEditor()
                                        background: Rectangle { color: addWorkingFrequencyButton.enabled && addWorkingFrequencyButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.18) : bgMedium; border.color: addWorkingFrequencyButton.enabled ? accentGreen : glassBorder; radius: 4 }
                                        contentItem: Text { text: addWorkingFrequencyButton.text; color: addWorkingFrequencyButton.enabled ? accentGreen : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Button {
                                        id: updateWorkingFrequencyButton
                                        text: qsTr("Update")
                                        enabled: settingsDialog.selectedWorkingFrequencyIndex >= 0
                                                 && settingsDialog.workingFrequencyEditorHasValidFrequency()
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 96
                                        onClicked: settingsDialog.updateWorkingFrequencyFromEditor()
                                        background: Rectangle { color: updateWorkingFrequencyButton.enabled && updateWorkingFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: updateWorkingFrequencyButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                                        contentItem: Text { text: updateWorkingFrequencyButton.text; color: updateWorkingFrequencyButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Button {
                                        id: deleteWorkingFrequencyButton
                                        text: qsTr("Delete")
                                        enabled: settingsDialog.selectedWorkingFrequencyIndex >= 0
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 96
                                        onClicked: settingsDialog.deleteSelectedWorkingFrequency()
                                        background: Rectangle { color: deleteWorkingFrequencyButton.enabled && deleteWorkingFrequencyButton.hovered ? Qt.rgba(1,0.2,0.2,0.16) : bgMedium; border.color: deleteWorkingFrequencyButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                                        contentItem: Text { text: deleteWorkingFrequencyButton.text; color: deleteWorkingFrequencyButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Button {
                                        id: clearWorkingFrequencyButton
                                        text: qsTr("New")
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 86
                                        onClicked: settingsDialog.newWorkingFrequencyEditor()
                                        background: Rectangle { color: clearWorkingFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.16) : bgMedium; border.color: glassBorder; radius: 4 }
                                        contentItem: Text { text: clearWorkingFrequencyButton.text; color: textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(420, Math.max(260, settingsDialog.height * 0.40))
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.62)
                            border.color: glassBorder
                            radius: 6
                            clip: true

                            Flickable {
                                id: frequencyTableFlick
                                anchors.fill: parent
                                contentWidth: Math.max(width, 1120)
                                contentHeight: frequencyTableColumn.height
                                boundsBehavior: Flickable.StopAtBounds
                                clip: true
                                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                Column {
                                    id: frequencyTableColumn
                                    width: frequencyTableFlick.contentWidth
                                    Rectangle {
                                        width: parent.width
                                        height: 30
                                        color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                                        Row {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            spacing: 8
                                            Text { text: qsTr("IARU Region"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("Mode"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 80; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("Frequency"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 210; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("Pref"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 56; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("Description"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 250; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("Start Date/Time"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: qsTr("End Date/Time"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                        }
                                    }
                                    Repeater {
                                        id: frequencySettingsList
                                        model: settingsDialog.workingFrequencyRows
                                        delegate: Rectangle {
                                            id: frequencySettingsRow
                                            width: frequencyTableColumn.width
                                            height: 30
                                            color: settingsDialog.selectedWorkingFrequencyIndex === Number(row.index)
                                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.22)
                                                   : (index % 2 === 0 ? Qt.rgba(1,1,1,0.035) : Qt.rgba(1,1,1,0.015))
                                            property var row: modelData
                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: settingsDialog.selectWorkingFrequencyRow(frequencySettingsRow.row)
                                            }
                                            Row {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10
                                                spacing: 8
                                                Text { text: frequencySettingsRow.row.region || ""; color: textPrimary; font.pixelSize: 11; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                Text { text: frequencySettingsRow.row.mode || ""; color: textPrimary; font.pixelSize: 11; width: 80; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                Text { text: frequencySettingsRow.row.frequency || ""; color: textPrimary; font.pixelSize: 11; width: 210; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                CheckBox {
                                                    id: preferredFrequencyCheck
                                                    checked: !!frequencySettingsRow.row.preferred
                                                    width: 56
                                                    height: 30
                                                    onClicked: {
                                                        bridge.setWorkingFrequencyPreferred(Number(frequencySettingsRow.row.index), checked)
                                                        settingsDialog.refreshFrequencySettings()
                                                    }
                                                    indicator: Rectangle { width: 14; height: 14; radius: 3; color: preferredFrequencyCheck.checked ? primaryBlue : bgMedium; border.color: glassBorder; x: preferredFrequencyCheck.width / 2 - width / 2; y: preferredFrequencyCheck.height / 2 - height / 2 }
                                                    contentItem: Text { text: "" }
                                                }
                                                Text { text: frequencySettingsRow.row.description || ""; color: textPrimary; font.pixelSize: 11; width: 250; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                Text { text: frequencySettingsRow.row.startTime || ""; color: textSecondary; font.pixelSize: 11; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                                Text { text: frequencySettingsRow.row.endTime || ""; color: textSecondary; font.pixelSize: 11; width: 170; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            spacing: 3
                            Text {
                                text: qsTr("STATION INFORMATION")
                                color: secondaryCyan
                                font.pixelSize: 12
                                font.bold: true
                                Layout.fillWidth: true
                            }
                            Text {
                                text: qsTr("Band offset is the transverter/station frequency offset for that band; use 0.000000 when unused.")
                                color: textSecondary
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 106
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.44)
                            border.color: glassBorder
                            radius: 6
                            clip: true
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text { text: qsTr("Band:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 46; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoComboBox {
                                        id: stationBandCombo
                                        model: settingsDialog.frequencyBandOptions
                                        Layout.preferredWidth: 132
                                        implicitHeight: controlHeight
                                        Component.onCompleted: settingsDialog.setComboText(stationBandCombo, "20m")
                                    }
                                    Text { text: qsTr("Offset MHz:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 82; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: stationOffsetField
                                        text: "0.000000"
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        horizontalAlignment: TextInput.AlignRight
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        Layout.preferredWidth: 146
                                        implicitHeight: controlHeight
                                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                    }
                                    Text { text: qsTr("Antenna:"); color: textSecondary; font.pixelSize: 11; Layout.preferredWidth: 70; Layout.preferredHeight: controlHeight; verticalAlignment: Text.AlignVCenter }
                                    DecoTextField {
                                        id: stationAntennaField
                                        color: textPrimary
                                        font.pixelSize: controlFontSize
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 160
                                        implicitHeight: controlHeight
                                        background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        id: addStationFrequencyButton
                                        text: qsTr("Add")
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 86
                                        onClicked: settingsDialog.addStationFrequencyFromEditor()
                                        background: Rectangle { color: addStationFrequencyButton.hovered ? Qt.rgba(accentGreen.r,accentGreen.g,accentGreen.b,0.18) : bgMedium; border.color: accentGreen; radius: 4 }
                                        contentItem: Text { text: addStationFrequencyButton.text; color: accentGreen; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Button {
                                        id: updateStationFrequencyButton
                                        text: qsTr("Update")
                                        enabled: settingsDialog.selectedStationFrequencyIndex >= 0
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 96
                                        onClicked: settingsDialog.updateStationFrequencyFromEditor()
                                        background: Rectangle { color: updateStationFrequencyButton.enabled && updateStationFrequencyButton.hovered ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.24) : bgMedium; border.color: updateStationFrequencyButton.enabled ? primaryBlue : glassBorder; radius: 4 }
                                        contentItem: Text { text: updateStationFrequencyButton.text; color: updateStationFrequencyButton.enabled ? primaryBlue : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Button {
                                        id: deleteStationFrequencyButton
                                        text: qsTr("Delete")
                                        enabled: settingsDialog.selectedStationFrequencyIndex >= 0
                                        implicitHeight: controlHeight
                                        Layout.preferredWidth: 96
                                        onClicked: settingsDialog.deleteSelectedStationFrequency()
                                        background: Rectangle { color: deleteStationFrequencyButton.enabled && deleteStationFrequencyButton.hovered ? Qt.rgba(1,0.2,0.2,0.16) : bgMedium; border.color: deleteStationFrequencyButton.enabled ? "#ff5b5b" : glassBorder; radius: 4 }
                                        contentItem: Text { text: deleteStationFrequencyButton.text; color: deleteStationFrequencyButton.enabled ? "#ff7777" : textSecondary; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 210
                            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.62)
                            border.color: glassBorder
                            radius: 6
                            clip: true
                            Column {
                                anchors.fill: parent
                                Rectangle {
                                    width: parent.width
                                    height: 30
                                    color: Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.18)
                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 10
                                        anchors.rightMargin: 10
                                        spacing: 8
                                        Text { text: qsTr("Band"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                        Text { text: qsTr("Offset"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: 160; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                        Text { text: qsTr("Antenna Description"); color: primaryBlue; font.pixelSize: 11; font.bold: true; width: parent.width - 310; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                                ListView {
                                    id: stationSettingsList
                                    width: parent.width
                                    height: parent.height - 30
                                    clip: true
                                    boundsBehavior: Flickable.StopAtBounds
                                    model: settingsDialog.stationFrequencyRows
                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                    delegate: Rectangle {
                                        id: stationSettingsRow
                                        width: stationSettingsList.width
                                        height: 30
                                        color: settingsDialog.selectedStationFrequencyIndex === Number(row.index)
                                               ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.22)
                                               : (index % 2 === 0 ? Qt.rgba(1,1,1,0.035) : Qt.rgba(1,1,1,0.015))
                                        property var row: modelData
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: settingsDialog.selectStationFrequencyRow(stationSettingsRow.row)
                                        }
                                        Row {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10
                                            spacing: 8
                                            Text { text: stationSettingsRow.row.band || ""; color: textPrimary; font.pixelSize: 11; width: 110; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: stationSettingsRow.row.offset || ""; color: textPrimary; font.pixelSize: 11; width: 160; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                            Text { text: stationSettingsRow.row.antenna || ""; color: textPrimary; font.pixelSize: 11; width: parent.width - 310; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                        }

                        Component.onCompleted: settingsDialog.refreshFrequencySettings()
                    }
                }

                // ═══════════ TAB 8 — COLORI ═══════════
                ScrollView {
                    id: colorsSettingsScroll
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    contentWidth: availableWidth
                    contentHeight: colorsSettingsGrid.implicitHeight + 34

                    GridLayout {
                        id: colorsSettingsGrid
                        width: Math.max(0, colorsSettingsScroll.availableWidth - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Colori Decodifica ──
                        Text { text: qsTr("DECODE COLORS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Repeater {
                            model: settingsDialog.decodeColorModel
                            delegate: RowLayout {
                                id: decodeColorRow
                                Layout.columnSpan: 4
                                Layout.fillWidth: true
                                spacing: 10
                                property string targetProp: modelData.prop
                                property string defaultColor: modelData.defaultColor
                                property string currentColor: bridge[targetProp] || defaultColor
                                property bool colorEnabled: bridge.decodeColorEnabled(targetProp)
                                property bool boldEnabled: bridge.decodeColorBold(targetProp)
                                property string shownColor: colorEnabled ? currentColor : bridge.decodeColorFallback
                                // 1.0.416 — sfondo riga per categoria (opt-in, default OFF)
                                property bool bgEnabled: bridge.decodeColorBgEnabled(targetProp)
                                property string bgColor: {
                                    var v = bridge.decodeColorBgValue(targetProp)
                                    return (v && v.length > 0) ? v : "#202830"
                                }

                                Text {
                                    text: modelData.label + ":"
                                    color: textSecondary
                                    font.pixelSize: 12
                                    Layout.preferredWidth: 210
                                    elide: Text.ElideRight
                                }

                                CheckBox {
                                    id: decodeColorEnabledCheck
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: controlHeight
                                    checked: decodeColorRow.colorEnabled
                                    onClicked: {
                                        decodeColorRow.colorEnabled = checked
                                        bridge.setDecodeColorEnabled(decodeColorRow.targetProp, checked)
                                        decodeColorInput.text = decodeColorRow.shownColor
                                    }
                                    indicator: Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 3
                                        color: parent.checked ? primaryBlue : bgMedium
                                        border.color: glassBorder
                                        y: parent.height / 2 - height / 2
                                    }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Use this specific decode color. When OFF, this category uses the shared default color.")
                                }

                                Rectangle {
                                    width: 60
                                    height: 24
                                    radius: 4
                                    color: settingsDialog.validHexColor(decodeColorRow.shownColor) ? decodeColorRow.shownColor : bridge.decodeColorFallback
                                    opacity: decodeColorRow.colorEnabled ? 1.0 : 0.55
                                    border.color: glassBorder
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: decodeColorRow.colorEnabled
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: decodeColorPresetPop.open()
                                    }
                                    Popup {
                                        id: decodeColorPresetPop
                                        width: 232
                                        height: 88
                                        background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                        Flow {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 4
                                            Repeater {
                                                model: settingsDialog.presetColors.concat([decodeColorRow.defaultColor])
                                                delegate: Rectangle {
                                                    width: 20
                                                    height: 20
                                                    radius: 3
                                                    color: modelData
                                                    border.color: glassBorder
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            settingsDialog.setDecodeHighlightColor(decodeColorRow.targetProp, modelData)
                                                            decodeColorInput.text = settingsDialog.normalizedHexColor(modelData)
                                                            decodeColorPresetPop.close()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                DecoTextField {
                                    id: decodeColorInput
                                    text: decodeColorRow.shownColor
                                    enabled: decodeColorRow.colorEnabled
                                    opacity: enabled ? 1.0 : 0.55
                                    selectByMouse: true
                                    implicitHeight: controlHeight
                                    Layout.preferredWidth: 110
                                    color: settingsDialog.validHexColor(text) ? textPrimary : "#ff5555"
                                    font.pixelSize: controlFontSize
                                    onActiveFocusChanged: {
                                        if (!activeFocus)
                                            text = decodeColorRow.shownColor
                                    }
                                    onAccepted: {
                                        if (settingsDialog.setDecodeHighlightColor(decodeColorRow.targetProp, text))
                                            text = settingsDialog.normalizedHexColor(text)
                                    }
                                    background: Rectangle {
                                        color: bgMedium
                                        border.color: decodeColorInput.activeFocus ? secondaryCyan : glassBorder
                                        radius: 4
                                    }
                                }

                                Button {
                                    text: qsTr("Reset")
                                    Layout.preferredWidth: 72
                                    implicitHeight: controlHeight
                                    enabled: decodeColorRow.colorEnabled
                                    opacity: enabled ? 1.0 : 0.55
                                    onClicked: {
                                        settingsDialog.setDecodeHighlightColor(decodeColorRow.targetProp, decodeColorRow.defaultColor)
                                        decodeColorInput.text = decodeColorRow.defaultColor
                                    }
                                    background: Rectangle {
                                        color: parent.hovered ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.24) : bgMedium
                                        border.color: glassBorder
                                        radius: 4
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: textPrimary
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Text { text: qsTr("Bold") + ":"; color: textSecondary; font.pixelSize: 11; Layout.leftMargin: 6 }
                                CheckBox {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: controlHeight
                                    checked: decodeColorRow.boldEnabled
                                    enabled: decodeColorRow.colorEnabled
                                    opacity: enabled ? 1.0 : 0.55
                                    onClicked: {
                                        decodeColorRow.boldEnabled = checked
                                        bridge.setDecodeColorBold(decodeColorRow.targetProp, checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height / 2 - height / 2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                // ── 1.0.416: colore di SFONDO riga (per categoria) ──
                                Text { text: qsTr("BG:"); color: textSecondary; font.pixelSize: 11; Layout.leftMargin: 8 }
                                CheckBox {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: controlHeight
                                    checked: decodeColorRow.bgEnabled
                                    onClicked: {
                                        decodeColorRow.bgEnabled = checked
                                        bridge.setDecodeColorBgEnabled(decodeColorRow.targetProp, checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height / 2 - height / 2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                    hoverEnabled: true
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: qsTr("Colors the row BACKGROUND (in addition to the text) for this category. OFF = no custom background.")
                                }
                                Rectangle {
                                    width: 60
                                    height: 24
                                    radius: 4
                                    color: settingsDialog.validHexColor(decodeColorRow.bgColor) ? decodeColorRow.bgColor : "#202830"
                                    opacity: decodeColorRow.bgEnabled ? 1.0 : 0.55
                                    border.color: glassBorder
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: decodeColorRow.bgEnabled
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: decodeBgPresetPop.open()
                                    }
                                    Popup {
                                        id: decodeBgPresetPop
                                        width: 232
                                        height: 88
                                        background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 6 }
                                        Flow {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 4
                                            Repeater {
                                                model: settingsDialog.presetColors
                                                delegate: Rectangle {
                                                    width: 20
                                                    height: 20
                                                    radius: 3
                                                    color: modelData
                                                    border.color: glassBorder
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var nz = settingsDialog.normalizedHexColor(modelData)
                                                            bridge.setDecodeColorBg(decodeColorRow.targetProp, nz)
                                                            decodeColorRow.bgColor = nz
                                                            decodeBgInput.text = nz
                                                            decodeBgPresetPop.close()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                DecoTextField {
                                    id: decodeBgInput
                                    text: decodeColorRow.bgColor
                                    enabled: decodeColorRow.bgEnabled
                                    opacity: enabled ? 1.0 : 0.55
                                    selectByMouse: true
                                    implicitHeight: controlHeight
                                    Layout.preferredWidth: 90
                                    color: settingsDialog.validHexColor(text) ? textPrimary : "#ff5555"
                                    font.pixelSize: controlFontSize
                                    onActiveFocusChanged: { if (!activeFocus) text = decodeColorRow.bgColor }
                                    onAccepted: {
                                        var nz = settingsDialog.normalizedHexColor(text)
                                        if (settingsDialog.validHexColor(nz)) {
                                            bridge.setDecodeColorBg(decodeColorRow.targetProp, nz)
                                            decodeColorRow.bgColor = nz
                                            text = nz
                                        }
                                    }
                                    background: Rectangle { color: bgMedium; border.color: decodeBgInput.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                        Text { text: qsTr("B4 Strikethrough:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.b4Strikethrough
                            onCheckedChanged: {
                                bridge.b4Strikethrough = checked
                                bridge.setSetting("b4Strikethrough", checked)
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text {
                            text: qsTr("Decode Boost:")
                            color: textSecondary
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
                            spacing: 10
                            Slider {
                                id: decodeColorBoostSlider
                                from: 0
                                to: 100
                                stepSize: 1
                                value: Math.max(0, Math.min(100, Number(bridge.getSetting("uiDecodeColorBoost", 35))))
                                Layout.fillWidth: true
                                onValueChanged: bridge.setSetting("uiDecodeColorBoost", Math.round(value))
                            }
                            Text {
                                text: Math.round(decodeColorBoostSlider.value) + "%"
                                color: textPrimary
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 44
                            }
                        }
                        Text {
                            text: qsTr("Visual contrast only; it does not change decoder sensitivity.")
                            color: textDim
                            font.pixelSize: 10
                            Layout.columnSpan: 4
                            Layout.leftMargin: 110
                        }

	                        // ── Highlighting ──
	                        Text { text: qsTr("HIGHLIGHTING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Highlight 73:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("Highlight73", true)
                            onCheckedChanged: bridge.setSetting("Highlight73", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("HL Orange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightOrange", false)
                            onCheckedChanged: bridge.setSetting("HighlightOrange", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Orange Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("HighlightOrangeCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("HighlightOrangeCallsigns", text.toUpperCase())
                        }

                        Text { text: qsTr("HL Blue:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("HighlightBlue", false)
                            onCheckedChanged: bridge.setSetting("HighlightBlue", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Blue Calls:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("HighlightBlueCallsigns", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("HighlightBlueCallsigns", text.toUpperCase())
                        }

                        // ── Colori Interfaccia (sfondo + testo) — #6, stile v3 ──
                        Text { text: qsTr("COLORI INTERFACCIA (sfondo + testo)"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        RowLayout {
                            Layout.columnSpan: 4; Layout.fillWidth: true; spacing: 10
                            Text { text: qsTr("Usa colori personalizzati:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
                            CheckBox {
                                checked: bridge.themeManager.customColorsEnabled
                                onToggled: bridge.themeManager.customColorsEnabled = checked
                                indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                contentItem: Text { text: ""; leftPadding: 24 }
                            }
                            Text { text: qsTr("(overrides theme background and text)"); color: textSecondary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                        }

                        RowLayout {
                            Layout.columnSpan: 4; Layout.fillWidth: true; spacing: 10
                            enabled: bridge.themeManager.customColorsEnabled
                            opacity: enabled ? 1.0 : 0.4
                            Text { text: qsTr("Background:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
                            Rectangle { width: 60; height: 24; radius: 4; border.color: glassBorder
                                color: settingsDialog.validHexColor(bridge.themeManager.customBgColor) ? bridge.themeManager.customBgColor : bgDeep }
                            DecoTextField {
                                id: customBgField
                                Layout.preferredWidth: 120; implicitHeight: 28; leftPadding: 8
                                text: bridge.themeManager.customBgColor
                                placeholderText: "#0A0F1A"
                                color: settingsDialog.validHexColor(text) ? textPrimary : "#ff5555"
                                font.pixelSize: 12; selectByMouse: true
                                onEditingFinished: if (settingsDialog.validHexColor(text)) bridge.themeManager.customBgColor = text
                                background: Rectangle { color: bgMedium; border.color: customBgField.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.columnSpan: 4; Layout.fillWidth: true; spacing: 10
                            enabled: bridge.themeManager.customColorsEnabled
                            opacity: enabled ? 1.0 : 0.4
                            Text { text: qsTr("Text:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 210; elide: Text.ElideRight }
                            Rectangle { width: 60; height: 24; radius: 4; border.color: glassBorder
                                color: settingsDialog.validHexColor(bridge.themeManager.customTextColor) ? bridge.themeManager.customTextColor : textPrimary }
                            DecoTextField {
                                id: customTextField
                                Layout.preferredWidth: 120; implicitHeight: 28; leftPadding: 8
                                text: bridge.themeManager.customTextColor
                                placeholderText: "#E8F4FD"
                                color: settingsDialog.validHexColor(text) ? textPrimary : "#ff5555"
                                font.pixelSize: 12; selectByMouse: true
                                onEditingFinished: if (settingsDialog.validHexColor(text)) bridge.themeManager.customTextColor = text
                                background: Rectangle { color: bgMedium; border.color: customTextField.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        // ── Spettro ──
                        Text { text: qsTr("SPECTRUM"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Palette:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: paletteCombo
                            model: ["SDR Classic","Raptor Green","Grayscale","SmartSDR","Hot (SDR#)","deskHPSDR","Aether Default","Aether BlueGreen","Aether Fire","Aether Plasma","FlexRadio"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Math.max(0, bridge.uiPaletteIndex)
                            onActivated: {
                                bridge.uiPaletteIndex = currentIndex
                                bridge.setSetting("uiPaletteIndex", currentIndex)
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: paletteCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("Black Level:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallBlackLevel", 15)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallBlackLevel", value)
                        }

                        Text { text: qsTr("Color Gain:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 0; to: 100; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallColorGain", 50)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallColorGain", value)
                        }

                        Text { text: qsTr("Contrast:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        Slider {
                            from: 10; to: 150; stepSize: 1; value: Number(bridge.getSetting("uiWaterfallContrast", 80)); Layout.fillWidth: true; Layout.columnSpan: 3
                            onValueChanged: bridge.setSetting("uiWaterfallContrast", value)
                        }

                        // ── Download Dati ──
                        Text { text: qsTr("DATA DOWNLOAD"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: ""; Layout.preferredWidth: 100 }
                        RowLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 10
                            Rectangle {
                                width: 170; height: controlHeight; radius: 4
                                opacity: bridge.ctyDatUpdating ? 0.65 : 1.0
                                color: dlCtyMA.containsMouse && !bridge.ctyDatUpdating ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.ctyDatUpdating ? "Download CTY.dat..." : "Download CTY.dat"
                                    color: primaryBlue
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: dlCtyMA
                                    anchors.fill: parent
                                    enabled: !bridge.ctyDatUpdating
                                    hoverEnabled: true
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        dataDownloadStatus = qsTr("Checking cty.dat...")
                                        dataDownloadIsError = false
                                        bridge.checkCtyDatUpdate()
                                    }
                                }
                            }
                            Rectangle {
                                width: 190; height: controlHeight; radius: 4
                                opacity: bridge.call3TxtUpdating ? 0.65 : 1.0
                                color: dlCall3MA.containsMouse && !bridge.call3TxtUpdating ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium
                                border.color: primaryBlue
                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.call3TxtUpdating ? qsTr("Download CALL3.TXT...") : qsTr("Download CALL3.TXT")
                                    color: primaryBlue
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    id: dlCall3MA
                                    anchors.fill: parent
                                    enabled: !bridge.call3TxtUpdating
                                    hoverEnabled: true
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        dataDownloadStatus = qsTr("Downloading CALL3.TXT...")
                                        dataDownloadIsError = false
                                        bridge.downloadCall3Txt()
                                    }
                                }
                            }
                        }
                        Text {
                            text: dataDownloadStatus.length > 0 ? dataDownloadStatus : qsTr("After clicking, a message with the outcome or error appears here.")
                            color: dataDownloadIsError ? "#ff5555" : (dataDownloadStatus.length > 0 ? secondaryCyan : textSecondary)
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.columnSpan: 4
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 4; Layout.preferredHeight: 24 }
                    }
                }

                // ═══════════ TAB 9 — AVANZATE ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Avvio ──
                        Text { text: qsTr("STARTUP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            implicitHeight: advancedStartupGrid.implicitHeight

                            GridLayout {
                                id: advancedStartupGrid
                                width: parent.width
                                columns: 4
                                columnSpacing: 14
                                rowSpacing: 10
                                property int checkWidth: 34
                                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                                Text { text: qsTr("Monitor OFF:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    enabled: false
                                    checked: false
                                    Component.onCompleted: settingsDialog.setBoolSettingIfChanged("MonitorOFF", false, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; opacity: parent.enabled ? 1.0 : 0.55; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Monitor Last:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("MonitorLastUsed", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("MonitorLastUsed", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Auto Astro:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.getSetting("AutoAstroWindow", false)
                                    onCheckedChanged: bridge.setSetting("AutoAstroWindow", checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("kHz no k:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("kHzWithoutK", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("kHzWithoutK", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Progress Red:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("ProgressBarRed", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("ProgressBarRed", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("High DPI:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("HighDPI", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("HighDPI", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Larger Tab:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.getSetting("LargerTabWidget", false)
                                    onCheckedChanged: bridge.setSetting("LargerTabWidget", checked)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Direct Visual:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("DirectVisualAudioCaptureUnsafe", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("DirectVisualAudioCaptureUnsafe", checked, false)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Fast visual panadapter. In legacy mode it may open a second audio capture; in normal mode it only increases the visual refresh rate. Default: OFF.")
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Low CPU:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedStartupGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedStartupGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.lowCpuModeEnabled
                                    onToggled: {
                                        bridge.lowCpuModeEnabled = checked
                                        settingsDialog.scheduleSettingsPersist()
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Profile for slow PCs: maximum 2 FT threads, slower waterfall, reduced early/deep decoding. Default: OFF.")
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text {
                                    text: qsTr("Reduces FT threads, waterfall refresh, and QML rendering during monitor/TX.")
                                    color: textSecondary
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        // ── Aggiornamenti dati ──
                        Text { text: qsTr("DATA UPDATES"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("LotW Users:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: 120; Layout.preferredHeight: controlHeight }
                        Text {
                            text: bridge.lotwUpdating ? qsTr("Updating...")
                                  : (bridge.lotwUserCount > 0 ? qsTr("%1 users").arg(bridge.lotwUserCount)
                                                              : qsTr("Not loaded"))
                            color: bridge.lotwUserCount > 0 ? accentGreen : textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlHeight
                        }
                        Button {
                            id: forceLotwUpdateButton
                            text: qsTr("Force Update")
                            enabled: !bridge.lotwUpdating
                            implicitHeight: controlHeight
                            Layout.minimumWidth: 170
                            Layout.preferredWidth: Math.max(170, implicitWidth + 12)
                            onClicked: bridge.forceUpdateLotwUsers()
                        }
                        Item { Layout.fillWidth: true; Layout.preferredHeight: controlHeight }

                        Text { text: qsTr("US States:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: 120; Layout.preferredHeight: controlHeight }
                        Text {
                            text: bridge.usStateDataUpdating ? qsTr("Updating...")
                                  : (bridge.usStateDataReady ? qsTr("%1 calls, %2 locators").arg(bridge.usStateGridCount).arg(bridge.usStateLocatorCount)
                                                             : qsTr("Not loaded"))
                            color: bridge.usStateDataReady ? accentGreen : textSecondary
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: true
                            Layout.preferredHeight: controlHeight
                        }
                        Button {
                            id: forceUsStateUpdateButton
                            text: qsTr("Force Update")
                            enabled: !bridge.usStateDataUpdating
                            implicitHeight: controlHeight
                            Layout.minimumWidth: 170
                            Layout.preferredWidth: Math.max(170, implicitWidth + 12)
                            onClicked: bridge.updateUsStateData()
                        }
                        Item { Layout.fillWidth: true; Layout.preferredHeight: controlHeight }

                        // ── Comportamento ──
                        Text { text: qsTr("BEHAVIOR"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            implicitHeight: advancedBehaviorGrid.implicitHeight

                            GridLayout {
                                id: advancedBehaviorGrid
                                width: parent.width
                                columns: 4
                                columnSpacing: 14
                                rowSpacing: 10
                                property int checkWidth: 34
                                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                                Text { text: qsTr("Quick Call:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("QuickCall", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("QuickCall", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Force Call 1st:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("ForceCallFirst", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("ForceCallFirst", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("VHF/UHF:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.vhfUhfFeatures
                                    onToggled: {
                                        bridge.vhfUhfFeatures = checked
                                        settingsDialog.scheduleSettingsPersist()
                                        bridge.setSetting("VHFUHF", checked)
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Wait Features:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("WaitFeaturesEnabled", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("WaitFeaturesEnabled", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Erase Band Act:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("erase_BandActivity", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("erase_BandActivity", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Clear DX Grid:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("clear_DXgrid", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("clear_DXgrid", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Clear DX Call:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("clear_DXcall", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("clear_DXcall", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("RX>TX after QSO:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("set_RXtoTX", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("set_RXtoTX", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }

                                Text { text: qsTr("Alt Erase Btn:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("AlternateEraseButtonBehavior", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("AlternateEraseButtonBehavior", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("No Btn Color:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedBehaviorGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedBehaviorGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("TxWarningDisabled", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("TxWarningDisabled", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                            }
                        }

                        // ── Modo Operativo ──
                        Text { text: qsTr("OPERATING MODE"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Item {
                            Layout.columnSpan: 4
                            Layout.fillWidth: true
                            implicitHeight: advancedOperatingGrid.implicitHeight

                            GridLayout {
                                id: advancedOperatingGrid
                                width: parent.width
                                columns: 4
                                columnSpacing: 14
                                rowSpacing: 10
                                property int checkWidth: 34
                                property real labelWidth: Math.max(190, (width - (checkWidth * 2) - (columnSpacing * 3)) / 2)

                                Text { text: qsTr("Fox Mode:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.foxMode
                                    onToggled: {
                                        bridge.foxMode = checked
                                        settingsDialog.scheduleSettingsPersist()
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Hound Mode:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: bridge.houndMode
                                    onToggled: {
                                        bridge.houndMode = checked
                                        settingsDialog.scheduleSettingsPersist()
                                    }
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("SuperFox:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("SuperFox", true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("SuperFox", checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                                Text { text: qsTr("Show OTP:"); color: textSecondary; font.pixelSize: 12; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.preferredWidth: advancedOperatingGrid.labelWidth; Layout.preferredHeight: controlHeight }
                                CheckBox {
                                    Layout.preferredWidth: advancedOperatingGrid.checkWidth; Layout.preferredHeight: controlHeight
                                    checked: settingsDialog.boolSetting("ShowOTP", false)
                                    onToggled: settingsDialog.setBoolSettingIfChanged("ShowOTP", checked, false)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                            }
                        }

                        // ── Contest ──
                        Text { text: qsTr("CONTEST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Activity:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoComboBox {
                            id: contestCombo
                            model: [qsTr("None"),"NA VHF","EU VHF",qsTr("Field Day"),"RTTY Roundup","WW DIGI",qsTr("Fox"),qsTr("Hound"),"ARRL Digi","Q65 Pileup"]; Layout.fillWidth: true; implicitHeight: controlHeight; Layout.columnSpan: 3
                            currentIndex: Math.max(0, Math.min(model.length - 1, bridge.specialOperationActivity))
                            onActivated: {
                                bridge.specialOperationActivity = currentIndex
                                settingsDialog.scheduleSettingsPersist()
                            }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            contentItem: Text { text: contestCombo.displayText; color: textPrimary; font.pixelSize: controlFontSize; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate { contentItem: Text { text: modelData; color: textPrimary; font.pixelSize: 12 }
                                background: Rectangle { color: parent.highlighted ? Qt.rgba(primaryBlue.r,primaryBlue.g,primaryBlue.b,0.3) : bgMedium } }
                            popup.background: Rectangle { color: bgDeep; border.color: glassBorder; radius: 4 }
                        }

                        Text { text: qsTr("FD Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("Field_Day_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Field_Day_Exchange", text.toUpperCase())
                        }
                        Text { text: qsTr("RTTY Exchange:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("RTTY_Exchange", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("RTTY_Exchange", text.toUpperCase())
                        }

                        Text { text: qsTr("Contest Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("Contest_Name", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Contest_Name", text.toUpperCase())
                        }
                        Text { text: qsTr("Indiv Name:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Individual_Contest_Name", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Individual_Contest_Name", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("NCCC Sprint:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("NCCC_Sprint", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("NCCC_Sprint", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // ── NTP Time Sync ──
                        Text { text: qsTr("NTP TIME SYNC"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enable NTP:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.ntpEnabled
                            onClicked: bridge.setSetting("NTPEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text {
                            text: bridge.ntpEnabled
                                  ? (bridge.ntpSynced ? "Synced" : "Syncing / waiting reply")
                                  : "Disabled"
                            color: bridge.ntpEnabled ? (bridge.ntpSynced ? accentGreen : "#FF9800") : textSecondary
                            font.pixelSize: 12
                            Layout.columnSpan: 2
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text { text: qsTr("Custom Server:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            id: ntpServerField
                            text: bridge.getSetting("NTPCustomServer", "")
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            implicitHeight: controlHeight
                            leftPadding: 8
                            color: textPrimary
                            font.pixelSize: controlFontSize
                            placeholderText: qsTr("Empty = automatic public servers")
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onEditingFinished: bridge.setSetting("NTPCustomServer", text.trim())
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: controlHeight
                            radius: 4
                            color: ntpSyncNowMouse.containsMouse && bridge.ntpEnabled
                                   ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25)
                                   : bgMedium
                            border.color: bridge.ntpEnabled ? secondaryCyan : glassBorder
                            opacity: bridge.ntpEnabled ? 1.0 : 0.55
                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Sync Now")
                                color: bridge.ntpEnabled ? textPrimary : textSecondary
                                font.pixelSize: 12
                                font.bold: bridge.ntpEnabled
                            }
                            MouseArea {
                                id: ntpSyncNowMouse
                                anchors.fill: parent
                                enabled: bridge.ntpEnabled
                                hoverEnabled: true
                                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: bridge.syncNtpNow()
                            }
                        }

                        Text {
                            text: qsTr("Leave the server empty to automatically use pool.ntp.org, Apple, Cloudflare, and Google.")
                            color: textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.columnSpan: 4
                        }
                        Text { text: qsTr("RF self-calibration:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("DecoSyncSelfCalEnabled", false)
                            onClicked: bridge.setSetting("DecoSyncSelfCalEnabled", checked)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Use received decode DT values only as a secondary time-sync hint after NTP/HTTPS is already locked. Default: OFF.")
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text {
                            text: qsTr("Secondary hint only; it cannot create the first time lock.")
                            color: textSecondary
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.columnSpan: 2
                        }

                        // ── ADV Decoding ──
                        Text { text: qsTr("ADV DECODING"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Auto Mode:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 2
                            Switch {
                                text: qsTr("AUTO - enable the 3 technologies when needed")
                                checked: bridge.advAutoModeEnabled
                                onToggled: {
                                    bridge.advAutoModeEnabled = checked
                                    settingsDialog.scheduleSettingsPersist()
                                }
                                contentItem: Text {
                                    text: parent.text; color: textPrimary; font.pixelSize: 12; font.bold: true
                                    leftPadding: parent.indicator.width + 8
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Text {
                                text: qsTr("When ON, the 3 features below are managed automatically. Trigger: Neural+Turbo when decodes < 2/slot for 4 slots. Coherent when Q65 SNR < -22 dB.")
                                color: "#888"; font.pixelSize: 10
                                wrapMode: Text.WordWrap; Layout.fillWidth: true
                                leftPadding: 8
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 12
                                visible: bridge.advAutoModeEnabled
                                Text { text: qsTr("Live state:"); color: "#888"; font.pixelSize: 10 }
                                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advNeuralSyncActive ? "#0f0" : "#444" }
                                Text { text: qsTr("Neural"); color: bridge.advNeuralSyncActive ? "#0f0" : "#666"; font.pixelSize: 10 }
                                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advTurboFeedbackActive ? "#0f0" : "#444" }
                                Text { text: qsTr("Turbo"); color: bridge.advTurboFeedbackActive ? "#0f0" : "#666"; font.pixelSize: 10 }
                                Rectangle { width: 8; height: 8; radius: 4; color: bridge.advCoherentAvgActive ? "#0f0" : "#444" }
                                Text { text: qsTr("Coherent"); color: bridge.advCoherentAvgActive ? "#0f0" : "#666"; font.pixelSize: 10 }
                            }
                        }

                        Text { text: qsTr("Coherent Avg:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 2
                            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
                            Switch {
                                text: qsTr("Coherent Average (Q65/JT65)")
                                checked: bridge.coherentAvgEnabled
                                onToggled: {
                                    bridge.coherentAvgEnabled = checked
                                    settingsDialog.scheduleSettingsPersist()
                                }
                                enabled: !bridge.advAutoModeEnabled
                                contentItem: Text {
                                    text: parent.text; color: textPrimary; font.pixelSize: 12
                                    leftPadding: parent.indicator.width + 8
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Text {
                                text: qsTr("Accumulates multi-slot averaging for Q65/JT65 decodes (+1-3 dB)")
                                color: "#888"; font.pixelSize: 10
                                wrapMode: Text.WordWrap; Layout.fillWidth: true
                                leftPadding: 8
                            }
                        }

                        Text { text: qsTr("Neural Sync:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 2
                            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
                            Switch {
                                text: qsTr("Neural Sync (FT8 OSD decoder)")
                                checked: bridge.neuralSyncEnabled
                                onToggled: {
                                    bridge.neuralSyncEnabled = checked
                                    settingsDialog.scheduleSettingsPersist()
                                }
                                enabled: !bridge.advAutoModeEnabled
                                contentItem: Text {
                                    text: parent.text; color: textPrimary; font.pixelSize: 12
                                    leftPadding: parent.indicator.width + 8
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Text {
                                text: qsTr("Forces OSD-aware FT8 decoding (+2-3 dB on borderline signals)")
                                color: "#888"; font.pixelSize: 10
                                wrapMode: Text.WordWrap; Layout.fillWidth: true
                                leftPadding: 8
                            }
                        }

                        Text { text: qsTr("Turbo Feedback:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100; opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0 }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.columnSpan: 3; spacing: 2
                            opacity: bridge.advAutoModeEnabled ? 0.5 : 1.0
                            Switch {
                                text: qsTr("Turbo Feedback (extended LDPC iterations)")
                                checked: bridge.turboFeedbackEnabled
                                onToggled: {
                                    bridge.turboFeedbackEnabled = checked
                                    settingsDialog.scheduleSettingsPersist()
                                }
                                enabled: !bridge.advAutoModeEnabled
                                contentItem: Text {
                                    text: parent.text; color: textPrimary; font.pixelSize: 12
                                    leftPadding: parent.indicator.width + 8
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Text {
                                text: qsTr("Extended LDPC iterations for marginal decode recovery")
                                color: "#888"; font.pixelSize: 10
                                wrapMode: Text.WordWrap; Layout.fillWidth: true
                                leftPadding: 8
                            }
                        }

                        // ── OTP ──
                        Text { text: qsTr("OTP"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("OTP Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.getSetting("OTPEnabled", false)
                            onCheckedChanged: bridge.setSetting("OTPEnabled", checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("OTP Seed:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("OTPSeed", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize; echoMode: TextInput.Password
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPSeed", text)
                        }

                        Text { text: qsTr("OTP Interval:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        SpinBox {
                            id: otpIntSpin
                            from: 1; to: 3600; value: Number(bridge.getSetting("OTPinterval", 1)); editable: true
                            implicitHeight: controlHeight; Layout.fillWidth: true
                            onValueChanged: bridge.setSetting("OTPinterval", value)
                            contentItem: TextInput { text: otpIntSpin.textFromValue(otpIntSpin.value, otpIntSpin.locale); color: textPrimary; font.pixelSize: controlFontSize; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: spinTextSidePadding; rightPadding: spinTextSidePadding; readOnly: !otpIntSpin.editable; validator: otpIntSpin.validator; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                            background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                        }
                        Text { text: qsTr("OTP URL:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField {
                            text: bridge.getSetting("OTPUrl", ""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8
                            color: textPrimary; font.pixelSize: controlFontSize
                            background: Rectangle { color: bgMedium; border.color: parent.activeFocus ? secondaryCyan : glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("OTPUrl", text)
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 4; Layout.preferredHeight: 80 }
                    }
                }

                // ═══════════ TAB 10 — ALERTS ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Audio Alerts ──
                        Text { text: qsTr("AUDIO ALERTS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Alerts Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertSoundsEnabled
                            onToggled: settingsDialog.setAlertEnabled(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Button {
                            text: qsTr("Test")
                            enabled: bridge.alertSoundsEnabled
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 28
                            onClicked: bridge.playAlert("MyCall")
                            background: Rectangle {
                                radius: 4
                                color: parent.enabled ? settingsDialog.bgMedium : settingsDialog.bgDark
                                border.color: parent.enabled ? settingsDialog.primaryBlue : settingsDialog.glassBorder
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? settingsDialog.primaryBlue : settingsDialog.textDim
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Item { Layout.fillWidth: true }

                        // Alert grid
                        Text { text: qsTr("CQ in Msg:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertOnCq
                            onToggled: settingsDialog.setAlertCq(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("My Call:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.alertOnMyCall
                            onToggled: settingsDialog.setAlertMyCall(checked)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New DXCC:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXCC", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXCC", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New DXCC Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXCCOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXCCOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_Grid", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_Grid", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New Grid Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_GridOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_GridOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New Continent:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_Continent", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_Continent", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("New Cont Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ContinentOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ContinentOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New CQ Zone:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_CQZ", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_CQZ", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("CQ Zone Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_CQZOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_CQZOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("New ITU Zone:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ITUZ", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ITUZ", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("ITU Zone Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_ITUZOB", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_ITUZOB", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("DX Call/Grid:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_DXcall", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_DXcall", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("QSY Message:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("alert_QSYmessage", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("alert_QSYmessage", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                    }
                }

                // ═══════════ TAB 11 — FILTRI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 4; columnSpacing: 10; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        // ── Blacklist ──
                        Text { text: qsTr("BLACKLIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Blacklisted", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Blacklisted", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        // Blacklist 1-12 (2 per row)
                        Text { text: qsTr("Blacklist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist1", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist2", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist3", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist4", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist5", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist6", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist7", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist8", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist9", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist10", text.toUpperCase()) }

                        Text { text: qsTr("Blacklist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist11", text.toUpperCase()) }
                        Text { text: qsTr("Blacklist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Blacklist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Blacklist12", text.toUpperCase()) }

                        // ── Whitelist ──
                        Text { text: qsTr("WHITELIST"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("Whitelisted", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("Whitelisted", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Whitelist 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist1", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist2", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist3", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist4", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist5", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist6", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist7", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist8", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist9", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist10", text.toUpperCase()) }

                        Text { text: qsTr("Whitelist 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist11", text.toUpperCase()) }
                        Text { text: qsTr("Whitelist 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Whitelist12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Whitelist12", text.toUpperCase()) }

                        // ── Always Pass ──
                        Text { text: qsTr("ALWAYS PASS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Enabled:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("AlwaysPass", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("AlwaysPass", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }

                        Text { text: qsTr("Always Pass 1:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass1",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass1", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 2:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass2",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass2", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 3:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass3",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass3", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 4:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass4",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass4", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 5:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass5",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass5", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 6:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass6",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass6", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 7:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass7",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass7", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 8:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass8",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass8", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 9:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass9",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass9", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 10:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass10",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass10", text.toUpperCase()) }

                        Text { text: qsTr("Always Pass 11:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass11",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass11", text.toUpperCase()) }
                        Text { text: qsTr("Always Pass 12:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        DecoTextField { text: bridge.getSetting("Pass12",""); Layout.fillWidth: true; implicitHeight: controlHeight; leftPadding: 8; color: textPrimary; font.pixelSize: controlFontSize; background: Rectangle { color: bgMedium; border.color: glassBorder; radius: 4 }
                            onTextChanged: bridge.setSetting("Pass12", text.toUpperCase()) }

                        // ── Territory ──
                        Text { text: qsTr("EXCLUDE TERRITORY"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Europe:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory1", "EU", ["EUROPE", "EUROPA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory1", "EU", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory1", "EU", ["EUROPE", "EUROPA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Africa:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory2", "AF", ["AFRICA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory2", "AF", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory2", "AF", ["AFRICA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Oceania:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory3", "OC", ["OCEANIA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory3", "OC", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory3", "OC", ["OCEANIA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("Asia:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory4", "AS", ["ASIA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory4", "AS", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory4", "AS", ["ASIA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("North America:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory5", "NA", ["NORTH AMERICA", "N. AMERICA", "N AMERICA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory5", "NA", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory5", "NA", ["NORTH AMERICA", "N. AMERICA", "N AMERICA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }
                        Text { text: qsTr("South America:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 120 }
                        CheckBox {
                            checked: settingsDialog.territorySettingMatches("Territory6", "SA", ["SOUTH AMERICA", "S. AMERICA", "S AMERICA"])
                            onToggled: settingsDialog.setTerritoryExcluded("Territory6", "SA", checked)
                            Component.onCompleted: settingsDialog.normalizeTerritorySetting("Territory6", "SA", ["SOUTH AMERICA", "S. AMERICA", "S AMERICA"])
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        // ── Opzioni Filtro ──
                        Text { text: qsTr("FILTER OPTIONS"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 4; Layout.topMargin: 10 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 4; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text { text: qsTr("Worked on Band:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersHideWorkedBand", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersHideWorkedBand", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Hide stations already worked on the current band.")
                        }

                        Text { text: qsTr("Worked Today:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersHideWorkedToday", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersHideWorkedToday", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Hide stations already logged today in UTC.")
                        }

                        Text { text: qsTr("Wait & Pounce:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: bridge.waitPounceActive
                            onToggled: {
                                bridge.waitPounceActive = checked
                                settingsDialog.scheduleSettingsPersist()
                            }
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                            hoverEnabled: true
                            ToolTip.visible: hovered
                            ToolTip.delay: 400
                            ToolTip.text: qsTr("Wait & Pounce listens for filtered CQ decodes, but it only starts a reply when TX/CQ is already armed by the operator.")
                        }
                        Text { text: qsTr("W&P Filters Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersForWaitAndPounceOnly", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersForWaitAndPounceOnly", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Text { text: qsTr("Calling Only:"); color: textSecondary; font.pixelSize: 12; Layout.preferredWidth: 100 }
                        CheckBox {
                            checked: settingsDialog.boolSetting("FiltersForWord2", false)
                            onToggled: settingsDialog.setBoolSettingIfChanged("FiltersForWord2", checked, false)
                            indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                            contentItem: Text { text: ""; leftPadding: 24 }
                        }

                        Item { Layout.fillWidth: true; Layout.columnSpan: 2 }
                    }
                }

                // ═══════════ TAB 12 — PULSANTI UI ═══════════
                ScrollView {
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    GridLayout {
                        id: uiButtonsGrid
                        width: Math.max(0, parent.width - settingsDialog.scrollLeftMargin - settingsDialog.scrollRightMargin)
                        columns: 2; columnSpacing: 28; rowSpacing: 8
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: settingsDialog.scrollLeftMargin
                        anchors.rightMargin: settingsDialog.scrollRightMargin
                        anchors.topMargin: settingsDialog.scrollTopMargin

                        readonly property var toolbarButtons: [
                            { label: qsTr("Monitor (MON / STOP)"),  key: "uiBtnMonitorVisible" },
                            { label: qsTr("Setup (⚙)"),         key: "uiBtnSetupVisible" },
                            { label: "REC",                          key: "uiBtnRecVisible" },
                            { label: "WAV",                          key: "uiBtnWavVisible" },
                            { label: "Log",                          key: "uiBtnLogVisible" },
                            { label: "Macro",                        key: "uiBtnMacroVisible" },
                            { label: "Astro",                        key: "uiBtnAstroVisible" },
                            { label: qsTr("Layout (window reset)"),  key: "uiBtnFooterResetVisible" },
                            { label: qsTr("History (decode history)"), key: "uiBtnFooterHistoryVisible" },
                            { label: "CAT",                          key: "uiBtnCatVisible" },
                            { label: qsTr("Async FT2 (A)"),          key: "uiAsyncIconVisible" },
                            { label: "PSK Reporter",                 key: "uiPskReporterToolbarVisible" },
                            { label: qsTr("DX Cluster (toolbar)"),   key: "uiDxClusterToolbarVisible" },
                            { label: qsTr("World Clock"),            key: "uiWorldClockVisible" }
                        ]

                        Text {
                            text: qsTr("Show or hide UI buttons as you prefer. Changes are immediate and saved automatically.")
                            color: textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap
                            Layout.columnSpan: 2; Layout.fillWidth: true; Layout.bottomMargin: 4
                        }

                        // ── Toolbar in alto ──
                        Text { text: qsTr("TOP TOOLBAR"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 4 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Repeater {
                            model: uiButtonsGrid.toolbarButtons
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text { text: modelData.label; color: textPrimary; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                                CheckBox {
                                    checked: settingsDialog.boolSetting(modelData.key, true)
                                    onToggled: settingsDialog.setBoolSettingIfChanged(modelData.key, checked, true)
                                    indicator: Rectangle { width: 18; height: 18; radius: 3; color: parent.checked ? primaryBlue : bgMedium; border.color: glassBorder; y: parent.height/2 - height/2 }
                                    contentItem: Text { text: ""; leftPadding: 24 }
                                }
                            }
                        }

                        // ── Ordine pulsanti toolbar (drag&drop) ──
                        Text { text: qsTr("TOOLBAR BUTTON ORDER"); color: secondaryCyan; font.pixelSize: 12; font.bold: true; Layout.columnSpan: 2; Layout.topMargin: 14 }
                        Rectangle { Layout.fillWidth: true; Layout.columnSpan: 2; height: 1; color: Qt.rgba(secondaryCyan.r,secondaryCyan.g,secondaryCyan.b,0.3) }

                        Text {
                            text: qsTr("Drag the top toolbar buttons (long-press) to reorder them. Use the button below to restore the default order.")
                            color: textSecondary; font.pixelSize: 12; wrapMode: Text.WordWrap
                            Layout.columnSpan: 2; Layout.fillWidth: true; Layout.topMargin: 2
                        }

                        Rectangle {
                            Layout.columnSpan: 2
                            Layout.topMargin: 4
                            implicitWidth: resetOrderLabel.implicitWidth + 28
                            implicitHeight: 30
                            radius: 4
                            color: resetOrderMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.12)
                            border.color: primaryBlue
                            border.width: 1

                            Text {
                                id: resetOrderLabel
                                anchors.centerIn: parent
                                text: qsTr("Restore default button order")
                                color: textPrimary
                                font.pixelSize: 12
                            }

                            MouseArea {
                                id: resetOrderMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                // Reset via il canale setting canonico: Main.onSettingValueChanged
                                // ricostruisce uiToolbarOrder dal default quando il valore è vuoto.
                                onClicked: if (bridge) bridge.setSetting("uiToolbarOrder", "")
                            }
                        }

                        // ── Ordine pulsanti TX panel (drag&drop) ──
                        Rectangle {
                            Layout.columnSpan: 2
                            Layout.topMargin: 6
                            implicitWidth: resetTxOrderLabel.implicitWidth + 28
                            implicitHeight: 30
                            radius: 4
                            color: resetTxOrderMA.containsMouse ? Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.25) : Qt.rgba(primaryBlue.r, primaryBlue.g, primaryBlue.b, 0.12)
                            border.color: primaryBlue
                            border.width: 1

                            Text {
                                id: resetTxOrderLabel
                                anchors.centerIn: parent
                                text: qsTr("Restore default TX panel order")
                                color: textPrimary
                                font.pixelSize: 12
                            }

                            MouseArea {
                                id: resetTxOrderMA
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                // Reset via il canale setting canonico: TxPanel.onSettingValueChanged
                                // ricostruisce uiTxPanelOrder dal default quando il valore è vuoto.
                                onClicked: if (bridge) bridge.setSetting("uiTxPanelOrder", "")
                            }
                        }

                        Item { Layout.fillWidth: true; Layout.columnSpan: 2; Layout.fillHeight: true }
                    }
                }

            } // StackLayout
        } // RowLayout
    } // contentItem
}
