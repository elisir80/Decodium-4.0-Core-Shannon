import QtQuick
import QtQuick.Controls

// Shared viewport for every Settings page.
//
// Qt's automatic ScrollView sizing is not reliable when a GridLayout is
// anchored to the viewport but its controls have larger minimum widths.  In
// that case the controls overflow while contentWidth remains equal to the
// viewport, so the horizontal bar has no range.  Measuring the first page
// item explicitly keeps both scroll axes usable on small screens, translated
// labels and high-DPI configurations.
Flickable {
    id: root

    default property alias pageData: pageCanvas.data

    property int pageLeftMargin: 10
    property int pageTopMargin: 10
    property int pageRightMargin: 12
    property int pageBottomMargin: 24
    property real minimumContentWidth: 0

    readonly property real availableWidth: width
    readonly property real availableHeight: height

    // Keep a stable content surface instead of relying on ScrollView's
    // automatic single-child sizing.  The latter creates circular bindings
    // when a GridLayout is anchored to the viewport and its implicit size is
    // also used as the scroll extent.
    readonly property Item pageSurface: Item {
        id: pageCanvas
        parent: root.contentItem
        width: root.contentWidth
        height: root.contentHeight
    }

    readonly property Item measuredItem: pageCanvas.children.length > 0
                                         ? pageCanvas.children[0]
                                         : null
    readonly property real measuredImplicitHeight: measuredItem
                                                    ? Math.max(0,
                                                               measuredItem.implicitHeight,
                                                               measuredItem.childrenRect.y
                                                               + measuredItem.childrenRect.height)
                                                    : 0

    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.AutoFlickIfNeeded
    contentWidth: Math.max(width,
                           minimumContentWidth)
    contentHeight: Math.max(height,
                            measuredImplicitHeight + pageTopMargin + pageBottomMargin)

    ScrollBar.horizontal: ScrollBar {
        policy: ScrollBar.AsNeeded
        interactive: true
        active: hovered || pressed || root.contentWidth > root.availableWidth + 0.5
    }
    ScrollBar.vertical: ScrollBar {
        policy: ScrollBar.AsNeeded
        interactive: true
        active: hovered || pressed || root.contentHeight > root.availableHeight + 0.5
    }
}
