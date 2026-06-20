/* FullSpectrumPanel — DX-Pedition Mode decode table (design §3.6)
 * Phase 2b (1.0.331): real Full Spectrum decode list for the DX-Pedition
 * workspace. Reads bridge.bandActivityModel (the SAME model the classic
 * inline period1Panel uses) via the GLOBAL bridge context — does NOT touch
 * the classic inline (Main.qml period1Panel stays byte-identical).
 *
 * Lesson 1.0.205: every delegate clause is guarded with `!modelData` first
 *   to avoid the TypeError flood during model-swap transients.
 * Lesson Fase 2a: do NOT shadow the global `bridge`; the `bridge` property
 *   below is default-bound to appEngine so it is never undefined under
 *   async Loader.
 * By IU8LMC
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    // Global context handle, default-bound (never shadow-undefined under async Loader).
    property var bridge: (typeof appEngine !== 'undefined' ? appEngine : null)

    // Theme tokens (Fase 1) with safe fallbacks — NO hardcoded UI hex.
    readonly property var tm: bridge ? bridge.themeManager : null
    readonly property color cAccent:   tm ? tm.accentColor    : "#19ff88"
    readonly property color cPanel:    tm ? tm.panelColor     : "#0d1310"
    readonly property color cBorder:   tm ? tm.borderColor    : "#1f2a22"
    readonly property color cBg:       tm ? tm.bgDeep         : "#050706"
    readonly property color cText:     tm ? tm.textPrimary    : "#d6dcd8"
    readonly property color cTextDim:  tm ? tm.textSecondary  : "#6c7872"
    readonly property color cCyan:     tm ? tm.secondaryColor : "#66e6ff"
    readonly property color cGrid:     tm ? tm.gridColor      : "#00d4b4"
    readonly property color cTx:       tm ? tm.txColor        : "#ff7a5c"
    readonly property color cLotw:     root.bridge ? root.bridge.effectiveDecodeColor("colorLotwUser") : "#ffffff"
    readonly property color cCq:       root.bridge ? root.bridge.effectiveDecodeColor("colorCQ") : root.cAccent

    readonly property int rowH: tm ? tm.densityRowHeight() : 22
    readonly property int fSize: tm ? tm.densityFontSize() : 12

    // Column widths (compact-aware like the classic panel).
    readonly property bool compact: width < 560
    readonly property int wUtc:  compact ? 60 : 80
    readonly property int wDb:   34
    readonly property int wDt:   compact ? 40 : 46
    readonly property int wFreq: 46
    readonly property int gap:   8
    readonly property int wDxcc: compact ? 0 : Math.min(180, Math.max(90, Math.round(width * 0.20)))

    function usStateLabel(entry) {
        if (!root.bridge || !root.bridge.showUsState || !entry || !entry.usState)
            return ""
        return String(entry.usState).trim().toUpperCase()
    }

    function dxccDisplayText(entry) {
        if (!entry)
            return ""
        var country = entry.dxCountry ? String(entry.dxCountry) : ""
        var state = usStateLabel(entry)
        if (country.length > 0 && state.length > 0)
            return country + " · " + state
        return country.length > 0 ? country : state
    }

    // Header column strip.
    Rectangle {
        id: colHdr
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 20
        color: Qt.rgba(root.cGrid.r, root.cGrid.g, root.cGrid.b, 0.14)
        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            spacing: 0
            Text { text: "UTC"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; Layout.preferredWidth: root.wUtc }
            Text { text: "dB"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDb }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "DT"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDt }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "FREQ"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wFreq }
            Item { Layout.preferredWidth: root.gap }
            Text { text: "MESSAGE"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; Layout.fillWidth: true }
            Text { visible: root.wDxcc > 0; text: "DXCC"; color: root.cGrid; font.pixelSize: 10; font.bold: true; font.family: "monospace"; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: root.wDxcc }
        }
    }

    // Decode list.
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: colHdr.bottom; bottom: parent.bottom }
        anchors.margins: 2
        clip: true
        spacing: 1
        cacheBuffer: 600
        reuseItems: true
        model: (root.bridge && root.bridge.bandActivityModel) ? root.bridge.bandActivityModel : null

        property bool followTail: true
        function snapTail() {
            if (followTail) positionViewAtEnd()
        }
        onCountChanged: Qt.callLater(snapTail)
        onDraggingChanged: if (dragging) followTail = false
        onContentYChanged: {
            // Re-arm tail-follow when the user scrolls back to the bottom.
            followTail = (contentHeight <= height + 2) || (contentY >= originY + contentHeight - height - 48)
        }
        Component.onCompleted: Qt.callLater(positionViewAtEnd)

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 8 }

        delegate: Rectangle {
            id: del
            // 1.0.205 guard: !modelData FIRST in every clause.
            readonly property var entry: modelData || ({})
            readonly property bool isSep: !!(modelData && modelData.isSeparator === true)
            width: list.width
            height: isSep ? 4 : root.rowH
            color: !modelData ? "transparent" :
                   isSep ? "transparent" :
                   (root.bridge && root.bridge.decodeHighlightUserBg(entry).length > 0) ? root.bridge.decodeHighlightUserBg(entry) :
                   entry.isTx ? Qt.rgba(root.cTx.r, root.cTx.g, root.cTx.b, 0.18) :
                   entry.isCQ ? Qt.rgba(root.cCq.r, root.cCq.g, root.cCq.b, 0.12) :
                   (index % 2 === 0) ? Qt.rgba(root.cText.r, root.cText.g, root.cText.b, 0.02)
                                     : Qt.rgba(root.cText.r, root.cText.g, root.cText.b, 0.05)
            radius: 2

            // Subtle separator line.
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
                    // Same native path the classic panel uses via decodePanel.handleDecodeDoubleClick.
                    root.bridge.processDecodeDoubleClick(
                        del.entry.message || "",
                        del.entry.time || "",
                        del.entry.db || "",
                        parseInt(del.entry.freq || "0"))
                }
            }

            RowLayout {
                visible: !del.isSep
                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
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
                Text {
                    text: !modelData ? "" : (del.entry.freq || "")
                    color: !modelData ? root.cTextDim : (del.entry.isTx ? "#f1c40f" : root.cCyan)
                    font.pixelSize: root.fSize; font.family: "monospace"
                    font.bold: !!modelData && del.entry.isTx === true
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: root.wFreq
                }
                Item { Layout.preferredWidth: root.gap }
                Text {
                    text: !modelData ? "" : (del.entry.displayMessage || del.entry.message || "")
                    color: !modelData ? root.cText : (del.entry.isCQ ? root.cCq : root.cText)
                    font.pixelSize: root.fSize; font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Item {
                    visible: root.wDxcc > 0
                    Layout.preferredWidth: root.wDxcc
                    Layout.fillHeight: true
                    Text {
                        anchors.fill: parent
                        anchors.rightMargin: del.entry.isLotw === true ? 11 : 0
                        text: !modelData ? "" : root.dxccDisplayText(del.entry)
                        color: root.cTextDim
                        font.pixelSize: root.fSize; font.family: "monospace"
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: !!modelData && del.entry.isLotw === true
                        width: 6
                        height: 6
                        radius: 3
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.cLotw
                        border.color: root.cTextDim
                        border.width: 1
                    }
                }
            }
        }

        // Empty state.
        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: "No decodes"
            color: root.cTextDim
            font.pixelSize: 12; font.italic: true
        }
    }
}
