import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root

    property var operations: null
    property var legacyStatistics: ({})
    property color borderColor: "#2a3950"
    property color primaryColor: "#3f7cff"
    property color accentColor: "#2ecc71"
    property color textColor: "#e5eefc"
    property color mutedColor: "#9db1c9"

    clip: true
    contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        width: root.availableWidth
        spacing: 7

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 420 ? 4 : 2
            columnSpacing: 5
            rowSpacing: 5
            Repeater {
                model: [
                    { label: qsTr("QSO"), key: "qsos", color: root.primaryColor },
                    { label: qsTr("Confirmed"), key: "confirmed", color: root.accentColor },
                    { label: qsTr("Calls"), key: "calls", color: "#44d7e8" },
                    { label: qsTr("DXCC"), key: "dxcc", color: "#f6c344" },
                    { label: qsTr("Grids"), key: "grids", color: "#44d7e8" },
                    { label: "POTA", key: "pota", color: "#74d66a" },
                    { label: "IOTA", key: "iota", color: "#44d7e8" },
                    { label: "WPX", key: "wpx", color: "#f0b94d" }
                ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 3
                    color: "#101a28"
                    border.width: 1
                    border.color: root.borderColor
                    Column {
                        anchors.centerIn: parent
                        spacing: 1
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: root.operations
                                ? Number(root.operations.scorecard[modelData.key] || 0) : 0
                            color: modelData.color
                            font.pixelSize: 13
                            font.bold: true
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: root.mutedColor
                            font.pixelSize: 8
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            radius: 3
            color: "#101a28"
            border.width: 1
            border.color: root.borderColor
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                Text {
                    text: qsTr("30-DAY COMPARISON")
                    color: root.primaryColor
                    font.pixelSize: 9
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: root.operations
                            ? qsTr("QSO  %1  (%2)")
                                  .arg(root.operations.comparison.currentQsos || 0)
                                  .arg(Number(root.operations.comparison.qsoDelta || 0) >= 0
                                       ? "+" + Number(root.operations.comparison.qsoDelta || 0)
                                       : Number(root.operations.comparison.qsoDelta || 0))
                            : ""
                        color: root.textColor
                        font.pixelSize: 9
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: root.operations
                            ? qsTr("Calls  %1  (%2)")
                                  .arg(root.operations.comparison.currentCalls || 0)
                                  .arg(Number(root.operations.comparison.callDelta || 0) >= 0
                                       ? "+" + Number(root.operations.comparison.callDelta || 0)
                                       : Number(root.operations.comparison.callDelta || 0))
                            : ""
                        color: root.textColor
                        font.pixelSize: 9
                    }
                }
            }
        }

        Text {
            text: qsTr("BREAKDOWN")
            color: root.primaryColor
            font.pixelSize: 9
            font.bold: true
        }

        Repeater {
            model: root.operations ? root.operations.chartData : []
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 1
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.preferredWidth: 68
                        text: modelData.group
                        color: root.mutedColor
                        font.pixelSize: 8
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.label
                        color: root.textColor
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        text: qsTr("%1 / %2 QSL")
                            .arg(modelData.worked)
                            .arg(modelData.confirmed)
                        color: root.mutedColor
                        font.pixelSize: 8
                    }
                }
                ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, root.operations
                                 ? Number(root.operations.scorecard.qsos || 1) : 1)
                    value: Number(modelData.worked || 0)
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !root.operations
                     || root.operations.chartData.length === 0
            text: qsTr("No statistics for the current logbook filters")
            color: root.mutedColor
            font.pixelSize: 9
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
