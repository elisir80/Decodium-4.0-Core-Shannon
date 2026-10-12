import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    readonly property var bridge: appEngine
    Material.theme: Material.Dark
    Material.accent: "#44d4df"
    Material.primary: "#182632"
    title: qsTr("SSB — PC microphone")
    width: 780; height: 850; minimumWidth: 640; minimumHeight: 650
    color: "#101923"
    palette.windowText: "#eaf1f8"
    palette.text: "#eaf1f8"
    palette.buttonText: "#eaf1f8"
    palette.button: "#263747"
    palette.base: "#182632"
    palette.highlight: "#17a6bb"
    property var voice: ssbModule
    property var controls: bridge.ssbControls
    property bool ready: bridge.mode === "SSB" && bridge.catConnected
    property string radioActionStatus: ""
    property bool syncingRadioMode: false
    function syncRadioModeFromCat() {
        syncingRadioMode = true
        var mode = (bridge.catMode || "").toString().toUpperCase()
        if (mode === "LSB" || mode === "DIGL" || mode === "DATA-L" || mode === "PKTLSB") {
            side.currentIndex = 1
            dataInput.checked = mode !== "LSB"
        } else if (mode === "USB" || mode === "DIGU" || mode === "DATA-U" || mode === "PKTUSB") {
            side.currentIndex = 0
            dataInput.checked = mode !== "USB"
        }
        syncingRadioMode = false
    }
    function updateRadioMode() { bridge.ssbSetRadioMode(dataInput.checked ? (side.currentIndex ? "DIGL" : "DIGU") : (side.currentIndex ? "LSB" : "USB")) }
    onClosing: voice.stop()
    onActiveChanged: if (!active) voice.stop()
    Component.onDestruction: voice.stop()
    onVisibleChanged: if (visible) { syncRadioModeFromCat(); bridge.ssbRefreshControls() }
    Component.onCompleted: syncRadioModeFromCat()
    Shortcut { sequence: "Escape"; onActivated: { voice.stop(); voiceKeyer.stop() } }

    // Barra spaziatrice = PTT: si trasmette finche' e' premuta. Con il cursore in
    // un campo di testo (frequenza) la barra scrive uno spazio e basta; con un
    // elenco aperto non si ruba il fuoco.
    readonly property bool popupOpen: side.popup.visible || mic.popup.visible
                                      || agcBox.popup.visible || rxAgc.popup.visible
    function isTextEntry(item) { return !!item && ("cursorPosition" in item) }
    onActiveFocusItemChanged: Qt.callLater(function () {
        var f = win.activeFocusItem
        if (!win.active || f === keyCatcher || win.isTextEntry(f) || win.popupOpen)
            return
        keyCatcher.forceActiveFocus()
    })
    Item {
        id: keyCatcher
        focus: true
        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
                event.accepted = true
                if (win.ready && controls.busy !== true && mic.currentIndex >= 0 && !voice.transmitting)
                    voice.start()
            }
        }
        Keys.onReleased: function (event) {
            if (event.key === Qt.Key_Space && !event.isAutoRepeat) {
                event.accepted = true
                voice.stop()
            }
        }
    }

    // Altri comandi di ricezione che la radio espone via CAT: compaiono solo
    // quelli che dichiara di saper leggere e scrivere.
    readonly property var extraLevels: [
        { key: "af",      label: qsTr("AF gain %"),            kind: "pct" },
        { key: "sql",     label: qsTr("Squelch %"),            kind: "pct" },
        { key: "nr",      label: qsTr("Noise reduction %"),    kind: "pct" },
        { key: "nb",      label: qsTr("Noise blanker %"),      kind: "pct" },
        { key: "comp",    label: qsTr("Speech compressor %"),  kind: "pct" },
        { key: "voxgain", label: qsTr("VOX gain %"),           kind: "pct" },
        { key: "ifshift", label: qsTr("IF shift Hz"),          kind: "int" },
        { key: "notchf",  label: qsTr("Notch Hz"),             kind: "int" },
        { key: "preamp",  label: qsTr("Preamp dB"),            kind: "list" },
        { key: "att",     label: qsTr("Attenuator dB"),        kind: "list" }
    ]
    readonly property var extraFuncs: [
        { key: "nbOn",   label: qsTr("Noise blanker") },
        { key: "nrOn",   label: qsTr("Noise reduction") },
        { key: "anfOn",  label: qsTr("Auto notch") },
        { key: "compOn", label: qsTr("Speech compressor") },
        { key: "voxOn",  label: qsTr("VOX") },
        { key: "lockOn", label: qsTr("Dial lock") }
    ]
    Dialog {
        id: powerOffDialog
        title: qsTr("Power off radio?")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        anchors.centerIn: parent
        Label {
            text: qsTr("This will switch off the radio connected through Hamlib CAT. The CAT session is kept open so you can try Power On afterward.")
            wrapMode: Text.WordWrap
            width: 340
        }
        onAccepted: if (bridge.hamlibCat) bridge.hamlibCat.requestRadioPowerOff()
    }
    Connections {
        target: bridge
        function onCatConnectedChanged() { if (!bridge.catConnected) voice.stop(); if(win.visible) { syncRadioModeFromCat(); bridge.ssbRefreshControls() } }
        function onCatModeChanged() { syncRadioModeFromCat() }
        function onModeChanged() { if (bridge.mode !== "SSB") voice.stop() }
        function onDecoPortUseRemoteChanged() { voice.stop(); bridge.ssbRefreshControls() }
    }
    Connections {
        target: bridge.hamlibCat
        function onRadioActionCompleted(action, ok, error) {
            win.radioActionStatus = ok
                ? (action.toLowerCase().indexOf("atu") >= 0 ? qsTr("Radio ATU command sent")
                   : action.toLowerCase().indexOf("on") >= 0 ? qsTr("Radio power-on command sent")
                   : qsTr("Radio power-off command sent"))
                : (error || qsTr("Radio command failed"))
        }
    }
    footer: Frame {
        background: Rectangle { color: "#101923" }
        padding: 14
        ColumnLayout {
            anchors.fill: parent
        Button {
            Layout.fillWidth: true; Layout.preferredHeight: 64
            text: voice.transmitting ? qsTr("TRANSMITTING — release to receive") : qsTr("HOLD TO TALK • PTT")
            font.bold: true; font.pixelSize: 20
            enabled: ready && controls.busy !== true && mic.currentIndex>=0
            onPressed: voice.start()
            onReleased: voice.stop()
            onCanceled: voice.stop()
            background: Rectangle { radius: 8; color: voice.transmitting ? "#b53238" : "#087f92"; opacity: parent.enabled ? 1 : .45 }
        }
        Label { text: voice.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
        Label { text: qsTr("Hold the space bar to talk (Esc stops)."); Layout.fillWidth: true; color: "#6f8294"; font.pixelSize: 12 }
        }
    }
    ScrollView {
        anchors.fill: parent; anchors.margins: 18
        contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 14
            RowLayout {
                Label { text: "SSB"; font.pixelSize: 30; font.bold: true; color: "#44d4df" }
                Label { text: bridge.decoPortUseRemote ? qsTr("REMOTE RADIO") : qsTr("LOCAL RADIO"); Layout.fillWidth: true }
                Label { text: bridge.catConnected ? bridge.catMode : qsTr("CAT disconnected"); color: bridge.catConnected ? "#77e7a9" : "#ffa971" }
            }
            RowLayout {
                TextField {
                    id: dial; Layout.fillWidth: true; font.pixelSize: 24
                    // Consolas is not installed by default on macOS/Linux and
                    // makes Qt rebuild its font-family aliases at startup.
                    font.family: (typeof decodiumMonoFontFamily !== "undefined" && decodiumMonoFontFamily)
                                 ? decodiumMonoFontFamily : "monospace"
                    text: (bridge.frequency / 1000000).toFixed(6)
                    enabled: ready && controls.busy !== true && !voice.transmitting
                    onAccepted: { bridge.cwTuneFrequency(Number(text.replace(",","."))*1000000); focus=false }
                    ToolTip.visible: hovered; ToolTip.text: qsTr("Frequency in MHz. Press Enter to tune.")
                }
                Label { text: "MHz" }
                Button { text: "−100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency-100) }
                Button { text: "+100 Hz"; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.cwTuneFrequency(bridge.frequency+100) }
            }
            RowLayout {
                ComboBox { id: side; model: ["USB", "LSB"]; enabled: ready && controls.busy !== true && !voice.transmitting; onActivated: win.updateRadioMode() }
                CheckBox { id: dataInput; text: qsTr("USB/data audio input"); checked: true; enabled: ready && controls.busy !== true && !voice.transmitting; onToggled: if(win.ready && !win.syncingRadioMode) win.updateRadioMode() }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Activate SSB"); visible: bridge.mode !== "SSB"; onClicked: bridge.mode="SSB" }
            }
            Label { text: qsTr("TX uses the radio audio output selected in Settings. USB/LSB requires the radio to accept PC audio; use data input when needed."); wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9" }
            GroupBox {
                title: qsTr("PC microphone • voice processing"); Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        id: mic; Layout.fillWidth: true; model: voice.microphones; textRole: "name"; valueRole: "id"
                        enabled: !voice.transmitting
                        currentIndex: { for(var i=0;i<voice.microphones.length;i++)if(voice.microphones[i].id===voice.microphone)return i;return -1 }
                        displayText: currentIndex < 0 ? qsTr("Select PC microphone…") : currentText
                        onActivated: voice.microphone=currentValue
                    }
                    RowLayout {
                        Label { text: qsTr("Mic gain") }
                        Slider { Layout.fillWidth: true; from: -20; to: 30; stepSize: 1; value: voice.gainDb; onMoved: voice.gainDb=value }
                        Label { text: voice.gainDb.toFixed(0)+" dB"; Layout.preferredWidth: 50 }
                        CheckBox { text: qsTr("Automatic gain"); checked: voice.automatic; onToggled: voice.automatic=checked }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice filter") }
                        SpinBox { from: 80; to: 500; stepSize: 20; value: voice.lowCut; onValueModified: voice.lowCut=value }
                        Label { text: "–" }
                        SpinBox { from: 1800; to: 3500; stepSize: 100; value: voice.highCut; onValueModified: voice.highCut=value }
                        Label { text: qsTr("Hz • limiter active"); Layout.fillWidth: true }
                    }
                    RowLayout {
                        Label { text: qsTr("Voice level") }
                        ProgressBar { from: 0; to: 1; value: voice.micPeak; Layout.fillWidth: true }
                        Label { text: voice.micPeak>.001 ? (20*Math.log(voice.micPeak)/Math.LN10).toFixed(0)+" dBFS" : "—"; Layout.preferredWidth: 65 }
                    }
                }
            }
            GroupBox {
                title: qsTr("Voice keyer • preregistered messages"); Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        text: qsTr("Hold Record while speaking, release to save. Play sends the message on SSB; Esc stops.")
                        wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
                    }
                    Repeater {
                        model: voiceKeyer.messages
                        delegate: RowLayout {
                            id: keyRow
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: "F" + (keyRow.modelData.index + 1); Layout.preferredWidth: 26; color: "#44d4df" }
                            TextField {
                                id: keyLabel
                                text: keyRow.modelData.label; maxLength: 12; Layout.preferredWidth: 110
                                onEditingFinished: voiceKeyer.setSlotText(keyRow.modelData.index, text, keyText.text)
                            }
                            TextField {
                                id: keyText
                                text: keyRow.modelData.text; placeholderText: qsTr("text of the message"); Layout.fillWidth: true
                                onEditingFinished: voiceKeyer.setSlotText(keyRow.modelData.index, keyLabel.text, text)
                            }
                            Button {
                                text: qsTr("Record")
                                enabled: !voiceKeyer.playing && !(voiceKeyer.recording && !pressed)
                                onPressed: voiceKeyer.startRecording(keyRow.modelData.index)
                                onReleased: voiceKeyer.stopRecording()
                                onCanceled: voiceKeyer.stopRecording()
                            }
                            Button {
                                text: qsTr("Play")
                                enabled: keyRow.modelData.hasAudio && ready && controls.busy !== true
                                         && !voiceKeyer.playing && !voiceKeyer.recording && !voice.transmitting
                                onClicked: voiceKeyer.play(keyRow.modelData.index)
                            }
                            Button {
                                text: qsTr("Clear")
                                enabled: keyRow.modelData.hasAudio && !voiceKeyer.playing && !voiceKeyer.recording
                                onClicked: voiceKeyer.clearSlot(keyRow.modelData.index)
                            }
                        }
                    }
                    Label { text: voiceKeyer.status; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                }
            }
            GroupBox {
                title: qsTr("Radio • CAT controls"); Layout.fillWidth: true
                GridLayout {
                    anchors.fill: parent; columns: 4; columnSpacing: 12
                    Label { text: qsTr("RF power %") }
                    SpinBox { from: 0; to: 100; value: controls.power === undefined ? 0 : Math.round(controls.power); enabled: ready && controls.busy !== true && !voice.transmitting && controls.powerSupported === true; onValueModified: bridge.ssbSetControl("power",value) }
                    Label { text: qsTr("RX filter Hz") }
                    SpinBox { from: 300; to: 4000; stepSize: 100; value: controls.filter === undefined ? 2400 : controls.filter; enabled: ready && controls.busy !== true && !voice.transmitting && controls.filterSupported === true; onValueModified: bridge.ssbSetControl("filter",value) }
                    Label { text: qsTr("Radio AGC") }
                    DecoComboBox {
                        id: agcBox
                        model: controls.agcModes || []; textRole: "name"; valueRole: "value"
                        enabled: ready && controls.busy !== true && !voice.transmitting && controls.agcSupported === true
                        currentIndex: { var a=controls.agcModes || [];for(var i=0;i<a.length;i++)if(a[i].value===controls.agc)return i;return -1 }
                        onActivated: {
                            // Read the numeric value from the selected model row.
                            // This avoids relying on the style ComboBox's currentValue
                            // property, which can briefly expose the row index while
                            // a refreshed AGC model is being installed.
                            var a = controls.agcModes || [];
                            var row = a[currentIndex];
                            var agcValue = row && row.value !== undefined ? Number(row.value) : Number(currentValue);
                            bridge.ssbSetControl("agc", agcValue);
                        }
                    }
                    Label { text: qsTr("RF gain %") }
                    SpinBox { from: 0; to: 100; value: controls.rf === undefined ? 0 : Math.round(controls.rf); enabled: ready && controls.busy !== true && !voice.transmitting && controls.rfSupported === true; onValueModified: bridge.ssbSetControl("rf",value) }
                    Label { text: qsTr("Radio mic gain %") }
                    SpinBox { from: 0; to: 100; value: controls.mic === undefined ? 0 : Math.round(controls.mic); enabled: ready && controls.busy !== true && !voice.transmitting && controls.micSupported === true; onValueModified: bridge.ssbSetControl("mic",value) }
                    Button { text: qsTr("Read radio"); Layout.columnSpan: 2; enabled: ready && controls.busy !== true && !voice.transmitting; onClicked: bridge.ssbRefreshControls() }
                    Button {
                        text: qsTr("ATU Tune")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Ask the radio to start its internal antenna tuner.")
                        onClicked: bridge.hamlibCat.requestAtuTune()
                    }
                    Button {
                        text: qsTr("Power on radio")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Ask the radio to power on through Hamlib CAT, if supported by the model.")
                        onClicked: bridge.hamlibCat.requestRadioPowerOn()
                    }
                    Button {
                        text: qsTr("Power off radio")
                        Layout.columnSpan: 2
                        enabled: ready && controls.busy !== true && !voice.transmitting
                                 && !bridge.decoPortUseRemote && bridge.hamlibCat && bridge.hamlibCat.connected
                        onClicked: powerOffDialog.open()
                    }
                    Label {
                        text: win.radioActionStatus
                        visible: text.length > 0
                        color: text.indexOf(qsTr("failed")) >= 0 ? "#ffa971" : "#77e7a9"
                        Layout.columnSpan: 4
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    Label { text: qsTr("AGC OFF enables manual reception gain using RF gain. Unavailable controls are disabled."); Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a9bac9" }
                    Label { text: controls.error || ""; visible: text.length>0; color: "#ffa971"; Layout.columnSpan: 4; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            GroupBox {
                title: qsTr("Receive • radio settings (read and set by CAT)"); Layout.fillWidth: true
                visible: ready && bridge.catConnected
                GridLayout {
                    anchors.fill: parent; columns: 2; columnSpacing: 16; rowSpacing: 4
                    Repeater {
                        model: win.extraLevels
                        delegate: RowLayout {
                            id: lvl
                            required property var modelData
                            readonly property bool canEdit: win.ready && controls.busy !== true && !voice.transmitting
                            readonly property var cur: controls[modelData.key]
                            visible: controls[modelData.key + "Supported"] === true
                            Layout.columnSpan: 2; Layout.fillWidth: true
                            Label { text: lvl.modelData.label; Layout.preferredWidth: 170 }
                            Slider {
                                visible: lvl.modelData.kind === "pct"; Layout.fillWidth: true
                                from: 0; to: 100; stepSize: 1
                                value: lvl.cur === undefined ? 0 : lvl.cur
                                enabled: lvl.canEdit
                                onPressedChanged: if (!pressed) bridge.ssbSetControl(lvl.modelData.key, Math.round(value))
                            }
                            Label {
                                visible: lvl.modelData.kind === "pct"; Layout.preferredWidth: 50
                                text: Math.round(lvl.cur === undefined ? 0 : lvl.cur) + " %"
                            }
                            SpinBox {
                                visible: lvl.modelData.kind === "int"
                                from: controls[lvl.modelData.key + "Min"] === undefined ? -1200 : controls[lvl.modelData.key + "Min"]
                                to: controls[lvl.modelData.key + "Max"] === undefined ? 1200 : controls[lvl.modelData.key + "Max"]
                                stepSize: 50
                                value: lvl.cur === undefined ? 0 : lvl.cur
                                enabled: lvl.canEdit
                                onValueModified: bridge.ssbSetControl(lvl.modelData.key, value)
                            }
                            ComboBox {
                                visible: lvl.modelData.kind === "list"
                                model: controls[lvl.modelData.key + "Values"] || []
                                enabled: lvl.canEdit
                                currentIndex: { var a = controls[lvl.modelData.key + "Values"] || []; for (var i = 0; i < a.length; i++) if (a[i] === lvl.cur) return i; return -1 }
                                displayText: currentIndex < 0 ? "—" : (model[currentIndex] === 0 ? qsTr("Off") : model[currentIndex] + " dB")
                                onActivated: { var a = controls[lvl.modelData.key + "Values"] || []; bridge.ssbSetControl(lvl.modelData.key, Number(a[currentIndex])) }
                            }
                            Item { Layout.fillWidth: true; visible: lvl.modelData.kind !== "pct" }
                        }
                    }
                    Repeater {
                        model: win.extraFuncs
                        delegate: CheckBox {
                            id: fn
                            required property var modelData
                            text: modelData.label
                            visible: controls[modelData.key + "Supported"] === true
                            checked: controls[modelData.key] === true
                            enabled: win.ready && controls.busy !== true && !voice.transmitting
                            onToggled: {
                                bridge.ssbSetControl(modelData.key, checked ? 1 : 0)
                                checked = Qt.binding(function () { return controls[fn.modelData.key] === true })
                            }
                        }
                    }
                }
            }
            RowLayout {
                Label { text: "ALC: "+(bridge.rigAlcValid ? Math.round(bridge.rigAlc)+" %" : "—"); font.pixelSize: 18 }
                Label { text: qsTr("RF: ")+(bridge.rigPowerWatts > 0 ? bridge.rigPowerWatts.toFixed(1)+" W" : "—"); Layout.fillWidth: true; font.pixelSize: 18 }
                Label { text: qsTr("SWR: ")+(bridge.rigSwr >= 1 ? bridge.rigSwr.toFixed(1) : "—"); font.pixelSize: 18 }
            }
            RowLayout {
                CheckBox { text: qsTr("Listen RX"); checked: bridge.decoPortMonitor; enabled: !voice.transmitting; onToggled: { bridge.decoPortMonitor=checked; if(checked)bridge.startRx() } }
                Slider { from: 0; to: 100; stepSize: 1; value: bridge.remoteRxVolume; onMoved: bridge.remoteRxVolume=value; Layout.fillWidth: true }
                Label { text: bridge.remoteRxVolume+" %" }
            }
            RowLayout {
                Label { text: qsTr("Listen AGC") }
                ComboBox {
                    id: rxAgc
                    model: [qsTr("Off"), qsTr("Auto"), qsTr("Fast"), qsTr("Medium"), qsTr("Slow"), qsTr("Long")]
                    currentIndex: bridge.ssbRxAgcMode
                    onActivated: bridge.ssbRxAgcMode = currentIndex
                }
                Label { text: qsTr("Threshold"); visible: bridge.ssbRxAgcMode > 1 }
                Slider {
                    visible: bridge.ssbRxAgcMode > 1; Layout.fillWidth: true
                    from: -120; to: -20; stepSize: 1; value: bridge.ssbRxAgcThreshold
                    onMoved: bridge.ssbRxAgcThreshold = value
                }
                Label { visible: bridge.ssbRxAgcMode > 1; text: bridge.ssbRxAgcThreshold + " dBFS"; Layout.preferredWidth: 80 }
                Item { Layout.fillWidth: true; visible: bridge.ssbRxAgcMode <= 1 }
                Label { visible: bridge.ssbRxAgcMode > 0; text: qsTr("Gain %1 dB").arg(bridge.ssbRxAgcGainDb.toFixed(0)); Layout.preferredWidth: 90 }
            }
            Label {
                text: qsTr("Software AGC on the listening audio only (from DECODIUM SDR). Auto follows the noise floor by itself. It does not touch the decoder or the radio's AGC.")
                wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#a9bac9"
            }
            ComboBox {
                Layout.fillWidth: true; model: bridge.cwMonitorOutputs(); textRole: "label"; valueRole: "id"
                enabled: !voice.transmitting
                currentIndex: { for(var i=0;i<model.length;i++)if(model[i].id===bridge.remoteRxDevice)return i;return -1 }
                onActivated: bridge.remoteRxDevice=currentValue
            }

        }
    }
}
