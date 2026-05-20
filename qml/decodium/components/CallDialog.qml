// 1.0.262 — CALL Dialog (fork-only iu8lmc)
// Pannello per chiamata diretta verso uno specifico callsign con retry/timeout,
// + sezione AutoCQ generico (max cycles, pausa fra cicli).
//
// Apertura via callButton in TxPanel.qml. Bridge backend:
//   - bridge.targetCallActive       (bool, read-only)
//   - bridge.targetCallSign         (QString, settable)
//   - bridge.targetCallMaxRetries   (int)
//   - bridge.targetCallTimeoutS     (int)
//   - bridge.targetCallPeriod       (int: 0=1st, 1=2nd, 2=alterna)
//   - bridge.targetCallPauseS       (int)
//   - bridge.targetCallRetryCount   (int, read-only)
//   - bridge.startTargetCall()      / stopTargetCall()
//
// AutoCQ generico (reuse esistente):
//   - bridge.autoCqMaxCycles        (0 = infinito)
//   - bridge.autoCqPauseSec
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: callDialog
    title: qsTr("Chiamate (CALL)")
    width: 460
    height: 580
    minimumWidth: 400
    minimumHeight: 520
    flags: Qt.Dialog | Qt.WindowCloseButtonHint
    color: "#1a1a2e"

    readonly property color cAccent:     "#3a9dff"  // primaryBlue
    readonly property color cGreen:      "#3acb6c"  // successGreen
    readonly property color cOrange:     "#ff9b3a"  // warningOrange
    readonly property color cText:       "#e8edf2"  // textPrimary
    readonly property color cMuted:      "#8a93a3"
    readonly property color cBorder:     "#3a3e58"
    readonly property color cFieldBg:    "#22243a"

    Rectangle {
        anchors.fill: parent
        color: callDialog.color

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            // ====== HEADER ======
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "📞  " + qsTr("Chiamata diretta")
                    color: callDialog.cText
                    font.pixelSize: 18
                    font.bold: true
                    Layout.fillWidth: true
                }
                Rectangle {
                    visible: bridge && bridge.targetCallActive
                    width: stateLabel.implicitWidth + 16
                    height: stateLabel.implicitHeight + 6
                    radius: 4
                    color: Qt.alpha(callDialog.cGreen, 0.25)
                    border.color: callDialog.cGreen
                    border.width: 1
                    Text {
                        id: stateLabel
                        anchors.centerIn: parent
                        text: qsTr("ATTIVA")
                        color: callDialog.cGreen
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }

            // ====== TARGET CALLSIGN ======
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: qsTr("Target callsign"); color: callDialog.cMuted; font.pixelSize: 12 }
                TextField {
                    id: targetField
                    Layout.fillWidth: true
                    text: bridge ? bridge.targetCallSign : ""
                    placeholderText: "es. F4CQS"
                    color: callDialog.cText
                    font.pixelSize: 18
                    font.bold: true
                    font.family: decodiumMonoFontFamily
                    selectByMouse: true
                    background: Rectangle {
                        color: callDialog.cFieldBg
                        border.color: targetField.activeFocus ? callDialog.cAccent : callDialog.cBorder
                        border.width: 1
                        radius: 4
                    }
                    onEditingFinished: if (bridge) bridge.targetCallSign = text
                    enabled: !bridge || !bridge.targetCallActive
                }
            }

            // ====== TENTATIVI + TIMEOUT ======
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8

                Text { text: qsTr("Tentativi max"); color: callDialog.cMuted; font.pixelSize: 12 }
                Text { text: qsTr("Timeout slot (s)"); color: callDialog.cMuted; font.pixelSize: 12 }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    SpinBox {
                        id: retriesSpin
                        from: 0; to: 999
                        value: bridge ? bridge.targetCallMaxRetries : 10
                        editable: true
                        enabled: !infiniteCheck.checked && (!bridge || !bridge.targetCallActive)
                        onValueModified: if (bridge) bridge.targetCallMaxRetries = value
                        Layout.preferredWidth: 100
                    }
                    CheckBox {
                        id: infiniteCheck
                        text: qsTr("∞")
                        checked: bridge && bridge.targetCallMaxRetries === 0
                        enabled: !bridge || !bridge.targetCallActive
                        onToggled: {
                            if (!bridge) return
                            if (checked) bridge.targetCallMaxRetries = 0
                            else bridge.targetCallMaxRetries = retriesSpin.value > 0 ? retriesSpin.value : 10
                        }
                        contentItem: Text {
                            text: infiniteCheck.text
                            color: callDialog.cText
                            font.bold: true
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                SpinBox {
                    id: timeoutSpin
                    from: 10; to: 600
                    value: bridge ? bridge.targetCallTimeoutS : 90
                    editable: true
                    enabled: !bridge || !bridge.targetCallActive
                    Layout.preferredWidth: 120
                    onValueModified: if (bridge) bridge.targetCallTimeoutS = value
                }
            }

            // ====== PERIODO FT8/FT4 ======
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: qsTr("Periodo FT8/FT4"); color: callDialog.cMuted; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14
                    RadioButton {
                        id: periodFirst
                        text: qsTr("1st (:00/:30)")
                        checked: bridge && bridge.targetCallPeriod === 0
                        enabled: !bridge || !bridge.targetCallActive
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 0
                        contentItem: Text {
                            text: periodFirst.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    RadioButton {
                        id: periodSecond
                        text: qsTr("2nd (:15/:45)")
                        checked: bridge && bridge.targetCallPeriod === 1
                        enabled: !bridge || !bridge.targetCallActive
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 1
                        contentItem: Text {
                            text: periodSecond.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                    RadioButton {
                        id: periodAlt
                        text: qsTr("Alterna")
                        checked: bridge && bridge.targetCallPeriod === 2
                        enabled: !bridge || !bridge.targetCallActive
                        onToggled: if (checked && bridge) bridge.targetCallPeriod = 2
                        contentItem: Text {
                            text: periodAlt.text
                            color: callDialog.cText
                            leftPadding: 22
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // ====== PAUSA FRA CICLI ======
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("Pausa fra cicli (s)")
                    color: callDialog.cMuted
                    font.pixelSize: 12
                    Layout.preferredWidth: 180
                }
                SpinBox {
                    id: pauseSpin
                    from: 0; to: 300
                    value: bridge ? bridge.targetCallPauseS : 0
                    editable: true
                    enabled: !bridge || !bridge.targetCallActive
                    Layout.preferredWidth: 100
                    onValueModified: if (bridge) bridge.targetCallPauseS = value
                }
            }

            // ====== TELEMETRIA RUNTIME ======
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: telemetryCol.implicitHeight + 16
                color: Qt.alpha(callDialog.cAccent, 0.08)
                border.color: Qt.alpha(callDialog.cAccent, 0.3)
                border.width: 1
                radius: 4
                visible: bridge && bridge.targetCallActive

                ColumnLayout {
                    id: telemetryCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4
                    Text {
                        text: qsTr("Stato: chiamando %1").arg(bridge ? bridge.targetCallSign : "")
                        color: callDialog.cAccent
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Text {
                        text: bridge ? qsTr("Tentativo %1 / %2").arg(bridge.targetCallRetryCount).arg(
                                  bridge.targetCallMaxRetries === 0 ? qsTr("∞") : bridge.targetCallMaxRetries) : ""
                        color: callDialog.cText
                        font.pixelSize: 12
                    }
                }
            }

            Item { Layout.fillHeight: true } // spacer

            // ====== SEZIONE AUTOCQ GENERICO (riusa esistente) ======
            Rectangle { Layout.fillWidth: true; height: 1; color: callDialog.cBorder }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text {
                    text: qsTr("AutoCQ generico (pulsante ACQ)")
                    color: callDialog.cMuted
                    font.pixelSize: 11
                    font.italic: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: qsTr("Max chiamate CQ (0=∞)")
                        color: callDialog.cMuted
                        font.pixelSize: 12
                        Layout.preferredWidth: 180
                    }
                    SpinBox {
                        from: 0; to: 999
                        value: bridge ? bridge.autoCqMaxCycles : 0
                        editable: true
                        Layout.preferredWidth: 100
                        onValueModified: if (bridge) bridge.autoCqMaxCycles = value
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        text: qsTr("Pausa fra cicli CQ (s)")
                        color: callDialog.cMuted
                        font.pixelSize: 12
                        Layout.preferredWidth: 180
                    }
                    SpinBox {
                        from: 0; to: 300
                        value: bridge ? bridge.autoCqPauseSec : 0
                        editable: true
                        Layout.preferredWidth: 100
                        onValueModified: if (bridge) bridge.autoCqPauseSec = value
                    }
                }
            }

            // ====== ACTION BUTTONS ======
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 8

                Button {
                    text: qsTr("Chiudi")
                    Layout.preferredWidth: 90
                    onClicked: callDialog.close()
                    background: Rectangle {
                        radius: 4
                        color: Qt.alpha(callDialog.cText, 0.08)
                        border.color: callDialog.cBorder
                        border.width: 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: callDialog.cText
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: qsTr("Stop")
                    Layout.preferredWidth: 90
                    enabled: bridge && bridge.targetCallActive
                    onClicked: if (bridge) bridge.stopTargetCall()
                    background: Rectangle {
                        radius: 4
                        color: parent.enabled ? Qt.alpha(callDialog.cOrange, 0.2) : Qt.alpha(callDialog.cText, 0.05)
                        border.color: parent.enabled ? callDialog.cOrange : callDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? callDialog.cOrange : callDialog.cMuted
                        font.pixelSize: 13
                        font.bold: parent.enabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: qsTr("▶ Start")
                    Layout.preferredWidth: 110
                    enabled: bridge && !bridge.targetCallActive && targetField.text.trim().length > 2
                    onClicked: {
                        if (!bridge) return
                        bridge.targetCallSign = targetField.text
                        bridge.startTargetCall()
                    }
                    background: Rectangle {
                        radius: 4
                        color: parent.enabled ? Qt.alpha(callDialog.cGreen, 0.25) : Qt.alpha(callDialog.cText, 0.05)
                        border.color: parent.enabled ? callDialog.cGreen : callDialog.cBorder
                        border.width: parent.enabled ? 2 : 1
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? callDialog.cGreen : callDialog.cMuted
                        font.pixelSize: 13
                        font.bold: parent.enabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
