/* SignalRxPanel — DX-Pedition Mode "Signal RX · QSO Lock" (design §3.7)
 * Phase 2b (1.0.331): large DX-call lock header + decode list filtered on the
 * RX frequency. Reads bridge.rxDecodeModel (the SAME model the classic inline
 * rxFreqPanel uses) via the GLOBAL bridge context. Classic inline untouched.
 *
 * Lesson 1.0.205: `!modelData` guard FIRST in every delegate clause.
 * Lesson Fase 2a: never shadow the global `bridge` (default-bound below).
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)

    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:   tm ? tm.accentColor    : "#19ff88"
    readonly property color cBorder:   tm ? tm.borderColor    : "#1f2a22"
    readonly property color cText:     tm ? tm.textPrimary    : "#d6dcd8"
    readonly property color cTextDim:  tm ? tm.textSecondary  : "#6c7872"
    readonly property color cBlue:     tm ? tm.primaryColor   : "#3aa0ff"
    readonly property color cCyan:     tm ? tm.secondaryColor : "#66e6ff"
    readonly property color cTx:       tm ? tm.txColor        : "#ff7a5c"
    readonly property color cLotw:     root.bridge ? root.bridge.effectiveDecodeColor("colorLotwUser") : "#ffffff"
    readonly property color cCq:       root.bridge ? root.bridge.effectiveDecodeColor("colorCQ") : root.cAccent

    readonly property int rowH: tm ? tm.densityRowHeight() : 22
    readonly property int fSize: tm ? tm.densityFontSize() : 12

    readonly property bool compact: width < 420
    readonly property int wUtc:  compact ? 58 : 76
    readonly property int wDb:   34
    readonly property int wDt:   compact ? 40 : 46
    readonly property int gap:   8

    function usStateLabel(entry) {
        if (!root.bridge || !root.bridge.showUsState || !entry || !entry.usState)
            return ""
        return String(entry.usState).trim().toUpperCase()
    }

    // --- QSO lock banner: large DX call (accent) -------------------------------
    Rectangle {
        id: lockBanner
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 48
        color: Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.10)
        RowLayout {
            anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
            spacing: 12
            ColumnLayout {
                spacing: 0
                Text {
                    text: "QSO LOCK"
                    color: root.cTextDim
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.6
                }
                Text {
                    text: {
                        var c = (root.bridge && root.bridge.dxCall) ? String(root.bridge.dxCall).trim() : ""
                        return c.length > 0 ? c : "—"
                    }
                    color: root.cAccent
                    font.pixelSize: 24; font.bold: true; font.family: "monospace"
                }
            }
            Item { Layout.fillWidth: true }
            ColumnLayout {
                spacing: 0
                Layout.alignment: Qt.AlignVCenter
                Text {
                    text: "GRID"
                    color: root.cTextDim
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.4
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
                Text {
                    text: {
                        var g = (root.bridge && root.bridge.dxGrid) ? String(root.bridge.dxGrid).trim() : ""
                        return g.length > 0 ? g : "—"
                    }
                    color: root.cCyan
                    font.pixelSize: 16; font.bold: true; font.family: "monospace"
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }
            }
        }
    }

    // Column header strip.
    Rectangle {
        id: colHdr
        anchors { left: parent.left; right: parent.right; top: lockBanner.bottom }
        height: 20
        color: Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.18)
        RowLayout {
            anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
            spacing: 0
            Text { text: "UTC"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: "monospace"; Layout.preferredWidth: root.wUtc }
            Text { text: "dB"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDb }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "DT"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDt }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "MESSAGE"; color: root.cBlue; font.pixelSize: 10; font.bold: true; font.family: "monospace"; Layout.fillWidth: true }
        }
    }

    // RX-filtered decode list.
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: colHdr.bottom; bottom: parent.bottom }
        anchors.margins: 2
        clip: true
        spacing: 1
        cacheBuffer: 600
        reuseItems: true
        model: (root.bridge && root.bridge.rxDecodeModel) ? root.bridge.rxDecodeModel : null

        property bool followTail: true
        onCountChanged: Qt.callLater(function() { if (followTail) positionViewAtEnd() })
        onDraggingChanged: if (dragging) followTail = false
        onContentYChanged: {
            followTail = (contentHeight <= height + 2) || (contentY >= originY + contentHeight - height - 48)
        }
        Component.onCompleted: Qt.callLater(positionViewAtEnd)

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 8 }

        delegate: Rectangle {
            id: del
            readonly property var entry: modelData || ({})
            readonly property bool isSep: !!(modelData && modelData.isSeparator === true)
            width: list.width
            height: isSep ? 4 : root.rowH
            color: !modelData ? "transparent" :
                   isSep ? "transparent" :
                   (root.bridge && root.bridge.decodeHighlightUserBg(entry).length > 0) ? root.bridge.decodeHighlightUserBg(entry) :
                   entry.isTx ? Qt.rgba(0.95, 0.77, 0.06, 0.28) :
                   entry.isMyCall ? Qt.rgba(0.96, 0.26, 0.21, 0.28) :
                   entry.isCQ ? Qt.rgba(root.cCq.r, root.cCq.g, root.cCq.b, 0.14) :
                   (index % 2 === 0) ? Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.07)
                                     : Qt.rgba(root.cBlue.r, root.cBlue.g, root.cBlue.b, 0.13)
            radius: 2

            Rectangle {
                visible: del.isSep
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left; anchors.right: parent.right
                anchors.leftMargin: 12; anchors.rightMargin: 12
                height: 1
                color: Qt.rgba(0.85, 0.25, 0.25, 0.5)
            }

            MouseArea {
                enabled: !!modelData && !del.isSep
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: function(mouse) {
                    if (!modelData || del.isSep || del.entry.isTx) return
                    if (!root.bridge) return
                    if (mouse.button === Qt.LeftButton) {
                        if (!root.bridge.holdTxFreq)
                            root.bridge.txFrequency = parseInt(del.entry.freq || "0")
                    } else if (mouse.button === Qt.RightButton) {
                        root.bridge.rxFrequency = parseInt(del.entry.freq || "0")
                    }
                }
                onDoubleClicked: function(mouse) {
                    if (!modelData || del.isSep || del.entry.isTx) return
                    if (!root.bridge || mouse.button !== Qt.LeftButton) return
                    if (!del.entry.message) return
                    root.bridge.processDecodeDoubleClick(
                        del.entry.message || "",
                        del.entry.time || "",
                        del.entry.db || "",
                        parseInt(del.entry.freq || "0"))
                }
            }

            RowLayout {
                visible: !del.isSep
                anchors { fill: parent; leftMargin: 6; rightMargin: 6 }
                spacing: 0
                Text {
                    text: !modelData ? "" : (del.entry.formattedTime || del.entry.time || "")
                    color: !modelData ? root.cTextDim : (del.entry.isTx ? "#f1c40f" : root.cTextDim)
                    font.pixelSize: root.fSize; font.family: "monospace"
                    Layout.preferredWidth: root.wUtc
                }
                Text {
                    text: !modelData ? "" : (del.entry.db || "")
                    color: !modelData ? root.cTextDim : (del.entry.snrColor || (del.entry.isTx ? "#f1c40f" : root.cTextDim))
                    font.pixelSize: root.fSize; font.family: "monospace"
                    font.bold: !!modelData && del.entry.isTx === true
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: root.wDb
                }
                Item { Layout.preferredWidth: root.gap }
                Text {
                    text: !modelData ? "" : (del.entry.dt || "")
                    color: !modelData ? root.cTextDim : (del.entry.isTx ? "#f1c40f" : root.cTextDim)
                    font.pixelSize: root.fSize; font.family: "monospace"
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: root.wDt
                }
                Item { Layout.preferredWidth: root.gap }
                Rectangle {
                    visible: !!modelData && del.entry.isLotw === true
                    Layout.preferredWidth: 6
                    Layout.preferredHeight: 6
                    Layout.alignment: Qt.AlignVCenter
                    radius: 3
                    color: root.cLotw
                    border.color: root.cTextDim
                    border.width: 1
                }
                Rectangle {
                    visible: root.usStateLabel(del.entry).length > 0
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 16
                    Layout.alignment: Qt.AlignVCenter
                    radius: 4
                    color: Qt.rgba(root.cCyan.r, root.cCyan.g, root.cCyan.b, 0.16)
                    border.color: root.cCyan
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: root.usStateLabel(del.entry)
                        color: root.cCyan
                        font.pixelSize: 9
                        font.bold: true
                        font.family: "monospace"
                    }
                }
                Text {
                    text: !modelData ? "" : (del.entry.displayMessage || del.entry.message || "")
                    color: !modelData ? root.cText :
                           del.entry.isMyCall ? root.cTx :
                           del.entry.isCQ ? root.cCq : root.cText
                    font.pixelSize: root.fSize; font.family: "monospace"
                    font.bold: !!modelData && del.entry.isMyCall === true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: "No RX-frequency decodes"
            color: root.cTextDim
            font.pixelSize: 12; font.italic: true
        }
    }
}
