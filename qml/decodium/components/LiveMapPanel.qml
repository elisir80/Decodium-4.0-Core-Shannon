import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Decodium 1.0

Rectangle {
    id: root
    required property var engine

    color: "transparent"
    property bool detachable: false
    property bool detached: false
    signal detachRequested()

    property color bgDeep: engine ? engine.themeManager.bgDeep : "#0b1220"
    property color primaryBlue: engine ? engine.themeManager.primaryColor : "#3f7cff"
    property color secondaryCyan: engine ? engine.themeManager.secondaryColor : "#00d8ff"
    property color accentGreen: engine ? engine.themeManager.accentColor : "#2ecc71"
    property color accentAmber: engine ? engine.themeManager.warningColor : "#f6c344"
    property color textPrimary: engine ? engine.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: engine ? engine.themeManager.textSecondary : "#9db1c9"
    property color glassBorder: engine ? engine.themeManager.glassBorder : "#2a3950"
    property var worldMap: worldMapLoader.item
    property var mapLayers: engine ? engine.mapIntelligenceService : null
    property var baseMapService: mapLayers ? mapLayers.baseMapService : null
    property var externalOverlays: mapLayers ? mapLayers.externalOverlayService : null
    property var mapOperations: mapLayers ? mapLayers.operationsService : null
    property bool gpuLiveMapEnabled: engine ? !!engine.getSetting("LiveMapUseGpu", true) : true
    property bool intelligencePanelRequested: width >= 760
    property bool showRosterPreferences: false
    property bool showRosterColumns: false
    property bool showRosterRules: false
    property var hoveredGridDetails: ({})
    property real hoveredGridX: 0
    property real hoveredGridY: 0
    property bool gridPreviewVisible: false
    property bool gridDetailsPinned: false
    property var hoveredGeographicDetails: ({})
    property real hoveredGeographicX: 0
    property real hoveredGeographicY: 0
    property bool geographicPreviewVisible: false
    property var selectedOperationalDetails: ({})
    property var selectedGeographicDetails: ({})
    property real selectedMapX: 0
    property real selectedMapY: 0
    property bool operationalDetailsVisible: false
    property bool geographicDetailsVisible: false
    property bool moonLocatePending: false
    readonly property bool compactIntelligencePanel: width < 760

    component LayerToggle: Rectangle {
        required property string label
        required property color activeColor
        property bool checked: false
        property string helpText: ""
        signal toggled(bool value)

        width: Math.max(54, layerLabel.implicitWidth + 22)
        height: 24
        radius: 4
        color: checked
            ? Qt.rgba(activeColor.r, activeColor.g, activeColor.b, 0.18)
            : (layerMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
        border.width: 1
        border.color: checked ? activeColor : root.glassBorder

        Text {
            id: layerLabel
            anchors.centerIn: parent
            text: parent.label
            color: parent.checked ? parent.activeColor : root.textSecondary
            font.pixelSize: 10
            font.bold: true
        }
        MouseArea {
            id: layerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.toggled(!parent.checked)
        }
        ToolTip.visible: layerMouse.containsMouse && helpText.length > 0
        ToolTip.text: helpText
        ToolTip.delay: 450
    }

    function coerceBool(value, fallback) {
        if (value === undefined || value === null)
            return !!fallback
        if (typeof value === "boolean")
            return value
        if (typeof value === "number")
            return value !== 0

        var text = String(value).trim().toLowerCase()
        if (text === "true" || text === "1" || text === "yes" || text === "on")
            return true
        if (text === "false" || text === "0" || text === "no" || text === "off")
            return false
        return !!fallback
    }

    function syncMapSettings() {
        if (!engine || !worldMap)
            return
        worldMap.setHomeGrid(engine.grid)
        worldMap.setBaseMapEnabled(true)
        worldMap.setGreylineEnabled(!!engine.getSetting("ShowGreyline", true))
        worldMap.setDistanceInMiles(root.coerceBool(engine.getSetting("Miles", false), false))
        worldMap.setBaseMapService(root.baseMapService)
        worldMap.setExternalOverlayService(root.externalOverlays)
        worldMap.setCoveragePushPins(root.mapLayers
                                     ? root.mapLayers.coveragePushPinsEnabled : false)
        worldMap.setTimeZoneOverlayEnabled(root.mapLayers
                                           ? root.mapLayers.timeZoneOverlayEnabled : false)
        root.syncOperations()
    }

    function syncOperations() {
        if (!worldMap)
            return
        var operationalMarkers = []
        var sourceMarkers = mapOperations
                ? (mapOperations.operationalMarkers || []) : []
        for (var markerIndex = 0; markerIndex < sourceMarkers.length; ++markerIndex)
            operationalMarkers.push(sourceMarkers[markerIndex])
        if (mapLayerEnabled("moon") && externalOverlays
                && externalOverlays.moonDataAvailable) {
            operationalMarkers.push({
                "id": "moon-subpoint",
                "type": "MOON",
                "reference": "MOON",
                "label": qsTr("MOON"),
                "longitude": Number(externalOverlays.moonSublunarLongitude),
                "latitude": Number(externalOverlays.moonSublunarLatitude),
                "azimuth": Number(externalOverlays.moonAzimuth),
                "elevation": Number(externalOverlays.moonElevation),
                "distanceKm": Number(externalOverlays.moonDistanceKm),
                "illumination": Number(externalOverlays.moonIllumination)
            })
        }
        var geographicFeatures = mapOperations
                ? (mapOperations.geographicFeatures || []) : []
        if (externalOverlays && externalOverlays.earthquakeFeatures
                && externalOverlays.earthquakeFeatures.length > 0)
            geographicFeatures = geographicFeatures.concat(externalOverlays.earthquakeFeatures)
        worldMap.setOperationalMarkers(operationalMarkers)
        worldMap.setGeographicFeatures(geographicFeatures)
        worldMap.setProjection(
            mapOperations ? mapOperations.mapProjection : "Equirectangular")
    }

    function showOperationalDetails(details, x, y) {
        selectedOperationalDetails = details || ({})
        selectedMapX = Number(x || 0)
        selectedMapY = Number(y || 0)
        geographicDetailsVisible = false
        operationalDetailsVisible = true
        if (mapOperations && details && details.type === "pota"
                && details.reference)
            mapOperations.selectPotaPark(details.reference)
    }

    function showGeographicDetails(details, x, y) {
        selectedGeographicDetails = details || ({})
        selectedMapX = Number(x || 0)
        selectedMapY = Number(y || 0)
        operationalDetailsVisible = false
        geographicDetailsVisible = true
    }

    function showGeographicPreview(details, x, y) {
        if (!details || details.type !== "earthquake")
            return
        hoveredGeographicDetails = details
        hoveredGeographicX = Number(x || 0)
        hoveredGeographicY = Number(y || 0)
        geographicPreviewVisible = true
    }

    function hideGeographicPreview() {
        geographicPreviewVisible = false
        hoveredGeographicDetails = ({})
    }

    function earthquakeSummary(details) {
        var magnitude = Number(details ? details.magnitude : NaN)
        var depth = Number(details ? details.depthKm : NaN)
        var parts = []
        if (isFinite(magnitude))
            parts.push("M " + magnitude.toFixed(1))
        if (isFinite(depth) && depth >= 0)
            parts.push(depth.toFixed(depth < 10 ? 1 : 0) + " km deep")
        return parts.join("  ·  ")
    }

    function captureMapScreenshot() {
        if (!mapOperations || !worldMapLoader.item)
            return
        var path = mapOperations.reserveScreenshotPath()
        if (!path || path.length === 0)
            return
        worldMapLoader.grabToImage(function(result) {
            if (!result.saveToFile(path))
                console.warn("Unable to save Live Map screenshot", path)
        })
    }

    function aimSelectedMarker() {
        if (!mapOperations || !engine || !selectedOperationalDetails)
            return
        var latitude = Number(selectedOperationalDetails.latitude)
        var longitude = Number(selectedOperationalDetails.longitude)
        var homeGrid = String(engine.grid || "")
        if (!isFinite(latitude) || !isFinite(longitude)
                || homeGrid.length < 4)
            return
        mapOperations.aimRotatorAt(
            latitude, longitude,
            Number(engine.latFromGrid(homeGrid)),
            Number(engine.lonFromGrid(homeGrid)))
    }

    function operationalValue(key) {
        var base = selectedOperationalDetails || ({})
        if (base.type === "pota" && mapOperations
                && mapOperations.selectedPotaPark
                && mapOperations.selectedPotaPark[key] !== undefined
                && String(mapOperations.selectedPotaPark[key]).length > 0)
            return mapOperations.selectedPotaPark[key]
        return base[key] !== undefined ? base[key] : ""
    }

    function overlayUpdatedText(updatedMs) {
        if (!updatedMs || updatedMs <= 0)
            return qsTr("not updated")
        return Qt.formatDateTime(new Date(updatedMs), "HH:mm:ss")
    }

    function mapLayerEnabled(layerId) {
        return !!(mapLayers && mapLayers.layerModel
                  && mapLayers.layerModel.layerEnabled(layerId))
    }

    function layerDescription(layerId) {
        if (layerId === "confirmed")
            return qsTr("Confirmed grids contain an imported ADIF QSO with QSL_RCVD=Y, LOTW_QSL_RCVD=Y or EQSL_QSL_RCVD=Y.")
        if (layerId === "psk")
            return qsTr("Receivers that reported hearing your callsign to PSK Reporter during the last hour. Decodium PSK upload does not need to be enabled.")
        if (layerId === "pota")
            return qsTr("Live Parks on the Air activator spots. Disabling this layer immediately removes all POTA markers from the map.")
        if (layerId === "states")
            return qsTr("United States state boundaries from the U.S. Census TIGER service. The map focuses on the United States when enabled.")
        if (layerId === "counties")
            return qsTr("United States county boundaries from the U.S. Census TIGER service. Zoom in after it loads to inspect individual counties.")
        if (layerId === "iota")
            return qsTr("Official IOTA Directory groups. Catalog positions are shown on the map; worked and confirmed status is applied only when the imported ADIF QSO contains an IOTA field.")
        if (layerId === "wpx")
            return qsTr("WPX prefixes derived from your imported ADIF log.")
        if (layerId === "moon")
            return qsTr("Moon visibility hemisphere, sublunar point and path from your station. The marker appears as soon as the ephemeris has been calculated.")
        if (layerId === "earthquakes")
            return qsTr("Global earthquakes of magnitude 2.5 or greater reported by USGS during the last day.")
        if (layerId === "wildfires")
            return qsTr("Open global wildfire events published by NASA EONET.")
        if (layerId === "offline")
            return qsTr("Offline mode uses the local Decodium Atlas and stops online base maps, PSK MQTT and external map feeds. ADIF, local cache and radio activity remain available.")
        return ""
    }

    function rosterStatusColor(status) {
        if (status === "NEW")
            return "#ffb347"
        if (status === "UNCONFIRMED")
            return "#f6c344"
        if (status === "CONFIRMED")
            return root.accentGreen
        return root.textSecondary
    }

    function rosterStatusFill(status) {
        if (status === "NEW")
            return "#24ffb347"
        if (status === "UNCONFIRMED")
            return "#24f6c344"
        if (status === "CONFIRMED")
            return "#242ecc71"
        return "#181d2a3b"
    }

    function openCallLookup(rawCall) {
        var raw = String(rawCall || "").trim()
        var segments = raw.split("/")
        var best = ""
        for (var i = 0; i < segments.length; ++i) {
            if (segments[i].length > best.length)
                best = segments[i]
        }
        var call = (best.length ? best : raw).toUpperCase()
            .replace(/[^A-Z0-9]/g, "")
        if (call.length === 0)
            return
        var provider = mapLayers ? mapLayers.callLookupProvider : "QRZ"
        var base = provider === "HamQTH"
            ? "https://www.hamqth.com/"
            : (provider === "QRZCQ"
               ? "https://www.qrzcq.com/call/" : "https://www.qrz.com/db/")
        Qt.openUrlExternally(base + call)
    }

    function requestPskData() {
        if (!visible || !engine || !mapLayers || !mapLayers.layerModel
                || !mapLayers.layerModel.layerEnabled("psk")
                || mapLayers.layerModel.layerEnabled("offline")
                || engine.pskHeardByFetching)
            return
        engine.fetchPskHeardBy()
    }

    function updateMoonOverlay() {
        if (!externalOverlays)
            return
        var moonEnabled = mapLayers && mapLayers.layerModel
            && mapLayers.layerModel.layerEnabled("moon")
        var stationGrid = engine ? String(engine.grid || "").trim() : ""
        if (!moonEnabled || !engine || stationGrid.length < 4) {
            externalOverlays.setMoonData(false, 0, 0, 0, 0, 0, 0)
            return
        }
        externalOverlays.updateMoonForStation(
            Number(engine.latFromGrid(stationGrid)),
            Number(engine.lonFromGrid(stationGrid)))
    }

    onMapLayersChanged: Qt.callLater(root.updateMoonOverlay)
    onExternalOverlaysChanged: Qt.callLater(root.updateMoonOverlay)

    function locateMoon() {
        if (!worldMap || !externalOverlays
                || !externalOverlays.moonDataAvailable)
            return
        worldMap.focusLocation(
            Number(externalOverlays.moonSublunarLongitude),
            Number(externalOverlays.moonSublunarLatitude),
            90, 54)
    }

    function coordinateText(value, positiveSuffix, negativeSuffix) {
        var coordinate = Number(value)
        return Math.abs(coordinate).toFixed(1) + "°"
            + (coordinate >= 0 ? positiveSuffix : negativeSuffix)
    }

    function statisticsDate(epoch) {
        var value = Number(epoch || 0)
        if (value <= 0)
            return qsTr("n/a")
        return Qt.formatDateTime(new Date(value), "yyyy-MM-dd")
    }

    function syncTxState() {
        if (!engine || !worldMap)
            return
        var txTargetCall = engine.currentTx === 6 ? "" : engine.dxCall
        var txTargetGrid = engine.currentTx === 6 ? "" : engine.dxGrid
        worldMap.setTransmitState(!!(engine.transmitting || engine.tuning),
                                  txTargetCall,
                                  txTargetGrid,
                                  engine.mode)
    }

    function syncCoverage() {
        if (!worldMap)
            return
        worldMap.setCoverageCells(mapLayers ? mapLayers.coverageCells : [])
    }

    function syncSpotPaths() {
        if (!worldMap || !mapLayers || !mapLayers.pskLayerEnabled)
            return
        var paths = mapLayers.spotPaths || []
        // The renderer keeps a bounded contact list.  These paths are a
        // recent animated view of reporter -> station directionality.
        for (var index = 0; index < Math.min(paths.length, 80); ++index) {
            var path = paths[index]
            var fromGrid = String(path.fromGrid || "")
            var toGrid = String(path.toGrid || "")
            if (fromGrid.length < 4 || toGrid.length < 4)
                continue
            worldMap.addContact("PSKPATH" + index + "_" + String(path.source || ""),
                                toGrid, fromGrid, 0)
        }
    }

    function rosterColumnValue(row, column) {
        if (!row)
            return ""
        if (column === "Grid") return row.grid || ""
        if (column === "Band") return row.band || ""
        if (column === "Mode") return row.mode || ""
        if (column === "SNR") return row.snr !== undefined ? String(row.snr) + " dB" : ""
        if (column === "DXCC") return row.dxcc || ""
        if (column === "Continent") return row.continent || ""
        if (column === "CQ zone") return row.cqZone ? "CQ " + row.cqZone : ""
        if (column === "ITU zone") return row.ituZone ? "ITU " + row.ituZone : ""
        if (column === "State") return row.state || ""
        if (column === "County") return row.county || ""
        if (column === "POTA") return row.pota || ""
        if (column === "IOTA") return row.iota || ""
        if (column === "WPX") return row.wpx || ""
        if (column === "LoTW age") return row.lotwAgeDays >= 0 ? "LoTW " + row.lotwAgeDays + "d" : ""
        if (column === "eQSL age") return row.eqslAgeDays >= 0 ? "eQSL " + row.eqslAgeDays + "d" : ""
        if (column === "OQRS") return row.oqrs ? "OQRS" : ""
        if (column === "Age") return row.ageMinutes >= 0 ? row.ageMinutes + "m" : ""
        if (column === "Source") return row.source || ""
        return ""
    }

    function rosterColumnSummary(row) {
        if (!row || !mapLayers)
            return ""
        var values = []
        var columns = mapLayers.rosterVisibleColumns || []
        for (var index = 0; index < columns.length; ++index) {
            var value = rosterColumnValue(row, columns[index])
            if (value.length > 0)
                values.push(value)
        }
        return values.join("  ·  ")
    }

    function showGridPreview(details, x, y) {
        if (!details || !details.grid || gridDetailsPinned)
            return
        hoveredGridDetails = details
        hoveredGridX = Number(x)
        hoveredGridY = Number(y)
        gridPreviewVisible = true
    }

    function hideGridPreview() {
        gridPreviewVisible = false
    }

    function pinGridDetails(details) {
        if (!details || !details.grid || !mapLayers)
            return
        hoveredGridDetails = details
        gridPreviewVisible = false
        gridDetailsPinned = true
        mapLayers.selectGrid(details.grid)
    }

    function closeGridDetails() {
        gridDetailsPinned = false
        if (mapLayers)
            mapLayers.clearGridSelection()
    }

    function focusSelectedGrid() {
        if (!worldMap || !engine || !mapLayers || !mapLayers.selectedGrid)
            return
        worldMap.focusLocation(
            Number(engine.lonFromGrid(mapLayers.selectedGrid)),
            Number(engine.latFromGrid(mapLayers.selectedGrid)),
            28, 18)
    }

    function scheduleRebuild() {
        if (!engine || !worldMap)
            return
        rebuildTimer.restart()
    }

    function decoderFeedAllowed() {
        if (!mapLayers)
            return true
        var source = String(mapLayers.sourceFilter || "All").trim().toLowerCase()
        return source === "all" || source === "decoder"
    }

    function updateConsumerReady() {
        if (!engine || !engine.setWorldMapConsumerReady)
            return
        engine.setWorldMapConsumerReady(root, !!root.visible && !!worldMap)
    }

    function initializeMap() {
        if (!worldMap)
            return
        worldMap.setActive(visible)
        root.syncMapSettings()
        root.syncCoverage()
        root.syncTxState()
        root.updateConsumerReady()
        if (visible)
            root.scheduleRebuild()
    }

    Timer {
        id: rebuildTimer
        // 1.0.209 — Debounce 1s. Era interval:0 (fires next tick) che con
        // 13 signal del bridge che invocavano scheduleRebuild() (decodeList,
        // rxDecodeList, transmitting, tuning, dxCall, dxGrid, currentTx,
        // txEnabled, qsoProgress, autoCqRepeat, mode, settingValue, grid)
        // significava clearContacts + replayWorldMapFeed (re-itera 500 entries
        // + addContact ognuno + paint) 2 volte/sec. Mappa mai stabile, "non
        // si vedeva" perche' sempre in rebuild. Ora rebuild totale solo
        // quando l'utente apre il pannello o cambia home grid; i nuovi
        // contact arrivano incrementali via onWorldMapContactAdded.
        interval: 1000
        repeat: false
        onTriggered: {
            if (!root.engine || !root.worldMap)
                return
            worldMap.clearContacts()
            root.syncMapSettings()
            root.syncCoverage()
            if ((!root.mapLayers || root.mapLayers.liveLayerEnabled)
                    && root.decoderFeedAllowed())
                root.engine.replayWorldMapFeed()
            root.syncSpotPaths()
            root.syncTxState()
        }
    }

    Timer {
        interval: 60000
        repeat: true
        triggeredOnStart: true
        running: root.visible && root.mapLayers && root.mapLayers.layerModel
                 && root.mapLayers.layerModel.layerEnabled("moon")
        onTriggered: root.updateMoonOverlay()
    }

    Timer {
        id: pskInitialFetchTimer
        interval: 1200
        repeat: false
        onTriggered: root.requestPskData()
    }

    Connections {
        target: root.mapLayers ? root.mapLayers.layerModel : null
        function onLayerToggled(layerId, enabled) {
        if (layerId === "moon") {
                root.moonLocatePending = enabled
                root.updateMoonOverlay()
            } else if (layerId === "psk" && enabled) {
                pskInitialFetchTimer.restart()
            }
        }
    }

    Component.onCompleted: {
        // 1.0.213 — pausa l'animation timer del widget legacy quando il
        // pannello non e' visibile (riduce sprechi CPU ~50% in idle dietro
        // ad altri tab/pop-out chiusi).
        root.initializeMap()
        root.updateMoonOverlay()
        pskInitialFetchTimer.restart()
    }
    Component.onDestruction: {
        if (engine && engine.setWorldMapConsumerReady)
            engine.setWorldMapConsumerReady(root, false)
    }
    onVisibleChanged: {
        if (worldMap)
            worldMap.setActive(visible)
        root.updateConsumerReady()
        if (visible) {
            scheduleRebuild()
            Qt.callLater(root.updateMoonOverlay)
            pskInitialFetchTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 2

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.16)
            radius: 4
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: root.detachable && !root.detached ? 30 : 6
                anchors.rightMargin: 6
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 8

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: secondaryCyan
                    opacity: root.visible ? 1.0 : 0.5
                }

                Text {
                    text: "Live Map"
                    font.pixelSize: 14
                    font.bold: true
                    color: secondaryCyan
                }

                Item { Layout.fillWidth: true }

                // 1.0.223 — Toolbar zoom + greyline. Rifatti come Rectangle
                // inline (no Loader+Component) per evitare il bug 1.0.221 in
                // cui Layout.preferredWidth/Height era sul template Component
                // ma non veniva propagato al Loader -> bottoni 0x0 = invisibili
                // al click. Ora ogni bottone e' un Rectangle diretto figlio
                // del RowLayout, le Layout attached funzionano.
                Rectangle {
                    id: zoomOutBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomOutMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomOutMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "−"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomOutMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomOutMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomOut(1.4)
                    }
                    ToolTip.visible: zoomOutMa.containsMouse
                    ToolTip.text: qsTr("Zoom out")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: zoomInBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: zoomInMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: zoomInMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: 14
                        font.bold: true
                        color: zoomInMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: zoomInMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.zoomIn(1.4)
                    }
                    ToolTip.visible: zoomInMa.containsMouse
                    ToolTip.text: qsTr("Zoom in")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: resetBtn
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    color: resetMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : "transparent"
                    border.color: resetMa.containsMouse ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "⌂"
                        font.pixelSize: 12
                        font.bold: true
                        color: resetMa.containsMouse ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: resetMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (root.worldMap) root.worldMap.resetView()
                    }
                    ToolTip.visible: resetMa.containsMouse
                    ToolTip.text: qsTr("Reset view (auto-fit)")
                    ToolTip.delay: 500
                }

                Rectangle {
                    id: greylineBtn
                    property bool greylineOn: engine ? !!engine.getSetting("ShowGreyline", true) : true
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 18
                    radius: 4
                    // 1.0.227 — Reference esplicita greylineBtn.greylineOn per chiudere
                    // scope chain ambiguity. Pre-1.0.227 alcune builds Qt6.11 lamentavano
                    // ReferenceError "greylineOn is not defined" su hover/repaint cycle
                    // (deja-vu 1.0.205 TypeError flood -> logger sync stalls main thread).
                    color: greylineMa.containsMouse
                        ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25)
                        : (greylineBtn.greylineOn ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.18) : "transparent")
                    border.color: (greylineMa.containsMouse || greylineBtn.greylineOn) ? secondaryCyan
                                  : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "☼"
                        font.pixelSize: 12
                        font.bold: true
                        color: (greylineMa.containsMouse || greylineBtn.greylineOn) ? secondaryCyan : textSecondary
                    }
                    MouseArea {
                        id: greylineMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            greylineBtn.greylineOn = !greylineBtn.greylineOn
                            if (root.engine)
                                root.engine.setSetting("ShowGreyline", greylineBtn.greylineOn)
                            if (root.worldMap)
                                root.worldMap.setGreylineEnabled(greylineBtn.greylineOn)
                        }
                    }
                    ToolTip.visible: greylineMa.containsMouse
                    ToolTip.text: qsTr("Toggle day/night greyline overlay")
                    ToolTip.delay: 500
                }

                Rectangle {
                    visible: root.detachable
                    Layout.preferredWidth: 42
                    Layout.preferredHeight: 18
                    radius: 4
                    color: liveMapDetachMA.containsMouse ? Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.25) : "transparent"
                    border.color: liveMapDetachMA.containsMouse ? secondaryCyan : Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: root.detached ? qsTr("Dock") : qsTr("POP")
                        font.pixelSize: 10
                        font.bold: true
                        color: liveMapDetachMA.containsMouse ? secondaryCyan : textSecondary
                    }

                    MouseArea {
                        id: liveMapDetachMA
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.detachRequested()
                    }

                    ToolTip.visible: liveMapDetachMA.containsMouse
                    ToolTip.text: root.detached ? qsTr("Dock Live Map") : qsTr("Detach Live Map")
                    ToolTip.delay: 500
                }

                Text {
                    text: engine && engine.grid ? engine.grid : ""
                    font.pixelSize: 10
                    color: textSecondary
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.78)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.22)
            border.width: 1
            radius: 4
            clip: true

            Flickable {
                id: layerFlickable
                anchors.fill: parent
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                contentWidth: layerRow.implicitWidth
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentWidth > width

                function showFirstControl() {
                    contentX = 0
                }

                Component.onCompleted: initialToolbarPosition.restart()
                onVisibleChanged: {
                    if (visible)
                        initialToolbarPosition.restart()
                }

                Timer {
                    id: initialToolbarPosition
                    interval: 350
                    repeat: false
                    onTriggered: layerFlickable.showFirstControl()
                }

                ScrollBar.horizontal: ScrollBar {
                    policy: layerFlickable.contentWidth > layerFlickable.width
                        ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Row {
                    id: layerRow
                    height: parent.height
                    spacing: 6

                    Repeater {
                        model: root.mapLayers ? root.mapLayers.layerModel : null

                        delegate: LayerToggle {
                            required property string layerId
                            required property string layerColor
                            required property bool layerEnabled
                            required property int layerCount

                            anchors.verticalCenter: parent.verticalCenter
                            activeColor: layerColor
                            checked: layerEnabled
                            helpText: root.layerDescription(layerId)
                            onToggled: function(value) {
                                if (root.mapLayers)
                                    root.mapLayers.layerModel.setLayerEnabled(layerId, value)
                            }
                        }
                    }

                    BusyIndicator {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 18
                        height: 18
                        running: (root.mapLayers && root.mapLayers.loading)
                                 || (root.externalOverlays
                                     && root.externalOverlays.loading)
                        visible: running
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.mapLayers
                            ? qsTr("%1 worked / %2 confirmed")
                                  .arg(root.mapLayers.workedGridCount)
                                  .arg(root.mapLayers.confirmedGridCount)
                            : ""
                        color: root.textSecondary
                        font.pixelSize: 10
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: root.mapOperations
                            ? root.mapOperations.dataViewMode : qsTr("Live")
                        activeColor: root.secondaryCyan
                        checked: true
                        helpText: qsTr("Cycle Live, Logbook and combined map views")
                        onToggled: function(value) {
                            if (root.mapOperations)
                                root.mapOperations.cycleDataView()
                        }
                    }

                    ComboBox {
                        id: projectionCombo
                        anchors.verticalCenter: parent.verticalCenter
                        width: 168
                        height: 24
                        model: root.mapOperations
                            ? root.mapOperations.availableProjections : []
                        currentIndex: root.mapOperations
                            ? Math.max(0, model.indexOf(
                                           root.mapOperations.mapProjection)) : 0
                        font.pixelSize: 9
                        onActivated: {
                            if (root.mapOperations)
                                root.mapOperations.mapProjection = currentText
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Map projection")
                    }

                    ComboBox {
                        id: presetCombo
                        anchors.verticalCenter: parent.verticalCenter
                        width: 112
                        height: 24
                        model: root.mapOperations
                            ? root.mapOperations.mapPresets : []
                        currentIndex: root.mapOperations
                            ? Math.max(0, model.indexOf(
                                           root.mapOperations.activeMapPreset)) : 0
                        font.pixelSize: 9
                        onActivated: {
                            if (root.mapOperations)
                                root.mapOperations.applyMapPreset(currentText)
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Map preset")
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("SHOT")
                        activeColor: root.secondaryCyan
                        helpText: qsTr("Save a screenshot of the map")
                        onToggled: function(value) {
                            root.captureMapScreenshot()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("ROSTER")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open the independent call roster")
                        onToggled: function(value) {
                            mapOperationsWindows.openRoster()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("STATS")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open detailed statistics")
                        onToggled: function(value) {
                            mapOperationsWindows.openStatistics()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: qsTr("COND")
                        activeColor: root.primaryBlue
                        helpText: qsTr("Open radio and propagation conditions")
                        onToggled: function(value) {
                            mapOperationsWindows.openConditions()
                        }
                    }

                    LayerToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        label: root.intelligencePanelRequested ? qsTr("HIDE DETAILS")
                                                               : qsTr("DETAILS")
                        activeColor: root.primaryBlue
                        checked: root.intelligencePanelRequested
                        onToggled: function(value) {
                            root.intelligencePanelRequested = value
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.5)
            border.color: Qt.rgba(secondaryCyan.r, secondaryCyan.g, secondaryCyan.b, 0.35)
            border.width: 1
            radius: 4
            clip: true

            Loader {
                id: worldMapLoader
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 2
                anchors.bottomMargin: 2
                anchors.leftMargin: 2
                anchors.rightMargin: intelligencePanel.visible && !root.compactIntelligencePanel
                    ? intelligencePanel.width + 8 : 2
                active: root.visible
                sourceComponent: root.gpuLiveMapEnabled ? gpuWorldMapComponent : painterWorldMapComponent
                onLoaded: {
                    root.initializeMap()
                    Qt.callLater(root.updateMoonOverlay)
                }
                onStatusChanged: {
                    root.updateConsumerReady()
                    if (status === Loader.Error && root.gpuLiveMapEnabled) {
                        console.warn("Live Map GPU component failed to load; falling back to CPU WorldMapItem")
                        root.gpuLiveMapEnabled = false
                    }
                }

                Component {
                    id: painterWorldMapComponent
                    WorldMapItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                        onCoverageCellHovered: function(details, x, y) {
                            root.showGridPreview(details, x, y)
                        }
                        onCoverageCellHoverEnded: root.hideGridPreview()
                        onCoverageCellClicked: function(details, x, y) {
                            root.pinGridDetails(details)
                        }
                        onOperationalMarkerClicked: function(details, x, y) {
                            root.showOperationalDetails(details, x, y)
                        }
                        onGeographicFeatureClicked: function(details, x, y) {
                            root.showGeographicDetails(details, x, y)
                        }
                        onGeographicFeatureHovered: function(details, x, y) {
                            root.showGeographicPreview(details, x, y)
                        }
                        onGeographicFeatureHoverEnded: root.hideGeographicPreview()
                    }
                }

                Component {
                    id: gpuWorldMapComponent
                    WorldMapGpuItem {
                        onContactClicked: function(call, grid) {
                            if (root.engine)
                                root.engine.processMapContactClick(call, grid)
                        }
                        onCoverageCellHovered: function(details, x, y) {
                            root.showGridPreview(details, x, y)
                        }
                        onCoverageCellHoverEnded: root.hideGridPreview()
                        onCoverageCellClicked: function(details, x, y) {
                            root.pinGridDetails(details)
                        }
                        onOperationalMarkerClicked: function(details, x, y) {
                            root.showOperationalDetails(details, x, y)
                        }
                        onGeographicFeatureClicked: function(details, x, y) {
                            root.showGeographicDetails(details, x, y)
                        }
                        onGeographicFeatureHovered: function(details, x, y) {
                            root.showGeographicPreview(details, x, y)
                        }
                        onGeographicFeatureHoverEnded: root.hideGeographicPreview()
                    }
                }
            }

            Rectangle {
                id: operationalDetailsCard
                visible: root.operationalDetailsVisible
                z: 10
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.selectedMapX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.selectedMapY + 14))
                width: Math.min(330, parent.width - 16)
                height: 172
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.operationalValue("color")
                    || root.secondaryCyan

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: (root.operationalValue("reference")
                                   || root.operationalValue("type")
                                   || qsTr("Map item")).toString().toUpperCase()
                            color: root.secondaryCyan
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            text: "×"
                            onClicked: {
                                root.operationalDetailsVisible = false
                                if (root.mapOperations)
                                    root.mapOperations.clearSelectedPotaPark()
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.operationalValue("name")
                              || root.operationalValue("label")
                              || root.operationalValue("call")
                              || qsTr("Operational marker")
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [
                            root.operationalValue("call"),
                            root.operationalValue("grid"),
                            root.operationalValue("frequency"),
                            root.operationalValue("mode")
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.textSecondary
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: root.operationalValue("comments")
                              || root.operationalValue("locationDesc")
                              || root.operationalValue("description")
                              || qsTr("Click CALL to start a QSO or ROTATE to aim the antenna.")
                        color: root.textSecondary
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: qsTr("CALL")
                            enabled: String(root.operationalValue("call")).length > 0
                            onClicked: root.engine.processMapRosterCall(
                                root.operationalValue("call"),
                                root.operationalValue("grid"))
                        }
                        Button {
                            text: "QRZ"
                            enabled: String(root.operationalValue("call")).length > 0
                            onClicked: root.openCallLookup(
                                root.operationalValue("call"))
                        }
                        Button {
                            text: qsTr("ROTATE")
                            enabled: root.mapOperations
                                     && root.mapOperations.rotatorEnabled
                            onClicked: root.aimSelectedMarker()
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                id: geographicDetailsCard
                visible: root.geographicDetailsVisible
                z: 9
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.selectedMapX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.selectedMapY + 14))
                width: Math.min(280, parent.width - 16)
                height: root.selectedGeographicDetails.type === "earthquake" ? 156 : 104
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.selectedGeographicDetails.color
                    || root.secondaryCyan

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedGeographicDetails.label
                                  || root.selectedGeographicDetails.county
                                  || root.selectedGeographicDetails.state
                                  || qsTr("Geographic area")
                            color: root.textPrimary
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        ToolButton {
                            text: "×"
                            onClicked: root.geographicDetailsVisible = false
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [
                            root.selectedGeographicDetails.type,
                            root.selectedGeographicDetails.state,
                            root.selectedGeographicDetails.county
                        ].filter(function(value) {
                            return String(value || "").length > 0
                        }).join("  ·  ")
                        color: root.textSecondary
                        font.pixelSize: 9
                        wrapMode: Text.Wrap
                    }
                    Text {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        text: root.earthquakeSummary(root.selectedGeographicDetails)
                        color: root.selectedGeographicDetails.color || root.secondaryCyan
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        text: root.selectedGeographicDetails.timeUtc || ""
                        color: root.textSecondary
                        font.pixelSize: 9
                    }
                    RowLayout {
                        visible: root.selectedGeographicDetails.type === "earthquake"
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: root.selectedGeographicDetails.tsunami
                                  ? qsTr("Tsunami flag reported") : ""
                            color: "#ffb56a"
                            font.pixelSize: 9
                        }
                        ToolButton {
                            visible: String(root.selectedGeographicDetails.url || "").length > 0
                            text: qsTr("USGS")
                            onClicked: Qt.openUrlExternally(root.selectedGeographicDetails.url)
                        }
                    }
                }
            }

            Rectangle {
                id: geographicHoverCard
                visible: root.geographicPreviewVisible
                         && root.hoveredGeographicDetails
                         && root.hoveredGeographicDetails.type === "earthquake"
                z: 8
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.hoveredGeographicX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.hoveredGeographicY + 14))
                width: Math.min(272, parent.width - 16)
                height: 94
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.hoveredGeographicDetails.color || "#ff9b4b"

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    Text {
                        width: parent.width
                        text: root.hoveredGeographicDetails.place
                              || root.hoveredGeographicDetails.label || qsTr("Earthquake")
                        color: root.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: root.earthquakeSummary(root.hoveredGeographicDetails)
                        color: root.hoveredGeographicDetails.color || "#ffb15f"
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        width: parent.width
                        text: root.hoveredGeographicDetails.timeUtc || ""
                        color: root.textSecondary
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                    Text {
                        text: qsTr("Click for event details")
                        color: root.textSecondary
                        font.pixelSize: 8
                    }
                }
            }

            Rectangle {
                id: gridHoverCard
                visible: root.gridPreviewVisible
                         && root.hoveredGridDetails
                         && !!root.hoveredGridDetails.grid
                z: 8
                x: Math.max(8, Math.min(parent.width - width - 8,
                                        worldMapLoader.x + root.hoveredGridX + 14))
                y: Math.max(8, Math.min(parent.height - height - 8,
                                        worldMapLoader.y + root.hoveredGridY + 14))
                width: 232
                height: root.hoveredGridDetails.split ? 108 : 94
                radius: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.96)
                border.width: 1
                border.color: root.secondaryCyan

                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 3
                    Text {
                        text: root.hoveredGridDetails.grid || ""
                        color: root.secondaryCyan
                        font.pixelSize: 13
                        font.bold: true
                    }
                    Text {
                        text: qsTr("%1 worked  %2 confirmed")
                            .arg(Number(root.hoveredGridDetails.workedCount || 0))
                            .arg(Number(root.hoveredGridDetails.confirmedCount || 0))
                        color: root.textPrimary
                        font.pixelSize: 10
                    }
                    Text {
                        text: qsTr("%1 live  %2 PSK  %3")
                            .arg(Number(root.hoveredGridDetails.activeCount || 0))
                            .arg(Number(root.hoveredGridDetails.pskCount || 0))
                            .arg(root.hoveredGridDetails.liveStatus || "")
                        color: root.textSecondary
                        font.pixelSize: 9
                    }
                    Text {
                        text: (root.hoveredGridDetails.historicalStatus || "")
                              + (root.hoveredGridDetails.ageSeconds !== undefined
                                 ? qsTr("  last %1 s ago")
                                       .arg(Number(root.hoveredGridDetails.ageSeconds))
                                 : "")
                        color: root.textSecondary
                        font.pixelSize: 8
                    }
                    Text {
                        visible: !!root.hoveredGridDetails.split
                        text: qsTr("Hovered half: %1")
                            .arg(root.hoveredGridDetails.splitSegment || qsTr("Combined"))
                        color: "#f6c344"
                        font.pixelSize: 8
                    }
                }
            }

            Rectangle {
                id: gridDetailsCard
                visible: root.gridDetailsPinned && root.mapLayers
                         && !!root.mapLayers.selectedGrid
                z: 7
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                width: Math.max(260, Math.min(root.detached ? 520 : 460,
                                               parent.width - 24))
                height: Math.max(190, Math.min(520, parent.height - 24))
                radius: 4
                clip: true
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b, 0.97)
                border.width: 1
                border.color: root.secondaryCyan
                Material.theme: Material.Dark
                Material.accent: root.primaryBlue
                Material.foreground: root.textPrimary
                Material.background: root.bgDeep

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 5

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: root.mapLayers
                                    ? qsTr("GRID %1").arg(root.mapLayers.selectedGrid)
                                    : ""
                                color: root.secondaryCyan
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: {
                                    var summary = root.mapLayers
                                        ? root.mapLayers.selectedGridSummary : ({})
                                    return qsTr("%1 QSO  %2 confirmed  %3 live  %4 PSK")
                                        .arg(Number(summary.workedCount || 0))
                                        .arg(Number(summary.confirmedCount || 0))
                                        .arg(Number(summary.activeCount || 0))
                                        .arg(Number(summary.pskCount || 0))
                                }
                                color: root.textSecondary
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }
                        }
                        BusyIndicator {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            running: root.mapLayers
                                     && root.mapLayers.gridDetailsLoading
                            visible: running
                        }
                        ToolButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 26
                            text: "◎"
                            onClicked: root.focusSelectedGrid()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Center this grid")
                        }
                        ToolButton {
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 26
                            text: "×"
                            onClicked: root.closeGridDetails()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Close grid details")
                        }
                    }

                    TabBar {
                        id: gridDetailsTabs
                        Layout.fillWidth: true
                        TabButton {
                            text: root.mapLayers
                                ? qsTr("LIVE %1").arg(root.mapLayers.selectedGridLive.length)
                                : qsTr("LIVE")
                            font.pixelSize: 9
                        }
                        TabButton {
                            text: root.mapLayers
                                ? qsTr("HISTORY %1").arg(root.mapLayers.selectedGridQsos.length)
                                : qsTr("HISTORY")
                            font.pixelSize: 9
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: gridDetailsTabs.currentIndex

                        Item {
                            ListView {
                                id: gridLiveList
                                anchors.fill: parent
                                clip: true
                                spacing: 3
                                model: root.mapLayers
                                    ? root.mapLayers.selectedGridLive : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: gridLiveList.width
                                    height: 62
                                    radius: 3
                                    color: index % 2 ? "#101a28" : "#0d2430"
                                    border.width: modelData.isCq ? 1 : 0
                                    border.color: modelData.isCq
                                        ? root.accentGreen : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 5
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid
                                                         ? "  " + modelData.grid : "")
                                                color: modelData.isCq
                                                    ? root.accentGreen
                                                    : root.textPrimary
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("%1  %2  %3 dB  %4  |  %5")
                                                    .arg(modelData.band || "-")
                                                    .arg(modelData.mode || "-")
                                                    .arg(modelData.snr)
                                                    .arg(modelData.source || "")
                                                    .arg(modelData.gridEvidence || qsTr("Station locator"))
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.message
                                                      || modelData.observedUtc || ""
                                                color: root.secondaryCyan
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        ToolButton {
                                            text: "☆"
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallWatched(
                                                modelData.call, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Watch this station")
                                        }
                                        ToolButton {
                                            text: "QRZ"
                                            font.pixelSize: 8
                                            enabled: !!modelData.call
                                            onClicked: root.openCallLookup(modelData.call)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Open callsign lookup")
                                        }
                                        Button {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Start QSO with this station")
                                        }
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !root.mapLayers
                                         || (!root.mapLayers.gridDetailsLoading
                                             && gridLiveList.count === 0)
                                text: qsTr("No recent traffic in this grid")
                                color: root.textSecondary
                                font.pixelSize: 10
                            }
                        }

                        Item {
                            ListView {
                                id: gridHistoryList
                                anchors.fill: parent
                                clip: true
                                spacing: 3
                                model: root.mapLayers
                                    ? root.mapLayers.selectedGridQsos : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: gridHistoryList.width
                                    height: 56
                                    radius: 3
                                    color: modelData.confirmed
                                        ? "#142a22"
                                        : (index % 2 ? "#101a28" : "#0d2430")
                                    border.width: modelData.confirmed ? 1 : 0
                                    border.color: modelData.confirmed
                                        ? root.accentGreen : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 6
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid
                                                         ? "  " + modelData.grid : "")
                                                color: modelData.confirmed
                                                    ? root.accentGreen
                                                    : root.textPrimary
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: qsTr("%1 %2  %3  %4  %5")
                                                    .arg(modelData.qsoDate || "")
                                                    .arg(modelData.timeOn || "")
                                                    .arg(modelData.band || "-")
                                                    .arg(modelData.mode || "-")
                                                    .arg(modelData.source || "")
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                        }
                                        Text {
                                            text: modelData.confirmed
                                                ? qsTr("CONFIRMED") : qsTr("WORKED")
                                            color: modelData.confirmed
                                                ? root.accentGreen
                                                : root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        Button {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                        }
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !root.mapLayers
                                         || (!root.mapLayers.gridDetailsLoading
                                             && gridHistoryList.count === 0)
                                text: qsTr("No QSO history in this grid")
                                color: root.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: intelligencePanel
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 6
                width: Math.min(root.detached ? 480 : 380, parent.width - 12)
                visible: root.intelligencePanelRequested
                z: 4
                color: Qt.rgba(root.bgDeep.r, root.bgDeep.g, root.bgDeep.b,
                               root.compactIntelligencePanel ? 0.97 : 0.92)
                border.color: Qt.rgba(root.secondaryCyan.r, root.secondaryCyan.g,
                                      root.secondaryCyan.b, 0.48)
                border.width: 1
                radius: 4
                clip: true
                Material.theme: Material.Dark
                Material.accent: root.primaryBlue
                Material.foreground: root.textPrimary
                Material.background: root.bgDeep

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: qsTr("Map Intelligence")
                            color: root.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }
                        Item { Layout.fillWidth: true }
                        BusyIndicator {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            running: (root.mapLayers && root.mapLayers.loading)
                                     || (root.externalOverlays
                                         && root.externalOverlays.loading)
                            visible: running
                        }
                        ToolButton {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 22
                            text: "×"
                            onClicked: root.intelligencePanelRequested = false
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Close details")
                        }
                    }

                    TabBar {
                        id: intelligenceTabs
                        Layout.fillWidth: true
                        TabButton { text: qsTr("MAP"); font.pixelSize: 9 }
                        TabButton { text: qsTr("ROSTER"); font.pixelSize: 9 }
                        TabButton { text: qsTr("LOGBOOK"); font.pixelSize: 9 }
                        TabButton { text: qsTr("STATS"); font.pixelSize: 9 }
                        TabButton { text: qsTr("AWARDS"); font.pixelSize: 9 }
                        TabButton {
                            text: root.mapLayers && root.mapLayers.unreadAlertCount > 0
                                ? qsTr("ALERTS %1").arg(root.mapLayers.unreadAlertCount)
                                : qsTr("ALERTS")
                            font.pixelSize: 9
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: intelligenceTabs.currentIndex

                        ScrollView {
                            id: mapControlsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: mapControlsScroll.availableWidth
                                spacing: 5

                                Text {
                                    text: qsTr("LAYERS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 4
                                    rowSpacing: 0
                                    Repeater {
                                        model: root.mapLayers ? root.mapLayers.layerModel : null
                                        delegate: CheckBox {
                                            required property string layerId
                                            required property string label
                                            required property string layerColor
                                            required property bool layerEnabled
                                            required property int layerCount
                                            Layout.fillWidth: true
                                            checked: layerEnabled
                                            text: qsTr("%1  %2").arg(label).arg(layerCount)
                                            font.pixelSize: 9
                                            palette.text: layerColor
                                            onToggled: {
                                                if (root.mapLayers)
                                                    root.mapLayers.layerModel
                                                        .setLayerEnabled(layerId, checked)
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("BASE MAP")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.baseMapService
                                            ? root.baseMapService.availableProviders : []
                                        currentIndex: root.baseMapService
                                            ? Math.max(0, model.indexOf(
                                                           root.baseMapService.provider)) : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.baseMapService)
                                                root.baseMapService.provider = currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Decodium Atlas is local. NASA GIBS and MapTiler use the network when Offline mode is disabled.")
                                    }
                                    CheckBox {
                                        Layout.preferredWidth: implicitWidth
                                        text: qsTr("Offline")
                                        checked: root.mapLayerEnabled("offline")
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.layerModel.setLayerEnabled(
                                                    "offline", checked)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Use the local atlas and stop online base maps, PSK MQTT and external overlays. Local logbook and radio data remain available.")
                                    }
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    visible: root.baseMapService
                                        && root.baseMapService.provider === "MapTiler satellite"
                                    text: root.baseMapService
                                        ? root.baseMapService.mapTilerApiKey : ""
                                    placeholderText: qsTr("MapTiler API key")
                                    echoMode: TextInput.Password
                                    font.pixelSize: 9
                                    onEditingFinished: {
                                        if (root.baseMapService)
                                            root.baseMapService.mapTilerApiKey = text
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.baseMapService
                                            ? root.baseMapService.status : ""
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        elide: Text.ElideRight
                                    }
                                    Button {
                                        text: root.baseMapService
                                            && root.baseMapService.loading
                                            ? qsTr("Loading...") : qsTr("Refresh")
                                        font.pixelSize: 8
                                        enabled: root.baseMapService
                                            && !root.baseMapService.loading
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.baseMapService.refresh()
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.baseMapService
                                        && root.baseMapService.attribution.length > 0
                                    text: root.baseMapService && root.baseMapService.attributionUrl.length > 0
                                        ? qsTr("Base: <a href=\"%1\">%2</a>")
                                              .arg(root.baseMapService.attributionUrl)
                                              .arg(root.baseMapService.attribution)
                                        : qsTr("Base: %1").arg(root.baseMapService.attribution)
                                    textFormat: Text.RichText
                                    onLinkActivated: Qt.openUrlExternally(link)
                                    color: root.textSecondary
                                    linkColor: root.secondaryCyan
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableBands : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableBands
                                                       .indexOf(root.mapLayers.bandFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.bandFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Band")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableModes : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableModes
                                                       .indexOf(root.mapLayers.modeFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.modeFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Mode")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availablePeriods : ["All time"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availablePeriods
                                                       .indexOf(root.mapLayers.periodFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.periodFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Period")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableSources : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableSources
                                                       .indexOf(root.mapLayers.sourceFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.sourceFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Source")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableContinents : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableContinents
                                                       .indexOf(root.mapLayers.continentFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.continentFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Continent")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableDxcc : ["All"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, root.mapLayers.availableDxcc
                                                       .indexOf(root.mapLayers.dxccFilter)) : 0
                                        font.pixelSize: 10
                                        onActivated: root.mapLayers.dxccFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("DXCC")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    CheckBox {
                                        text: qsTr("CQ only")
                                        checked: root.mapLayers ? root.mapLayers.cqOnly : false
                                        font.pixelSize: 9
                                        onToggled: root.mapLayers.cqOnly = checked
                                    }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        text: root.engine && root.engine.pskHeardByFetching
                                            ? qsTr("Loading PSK...")
                                            : qsTr("Refresh PSK")
                                        font.pixelSize: 9
                                        enabled: root.engine && !root.engine.pskHeardByFetching
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.engine.fetchPskHeardBy()
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("Unavailable while Offline mode is enabled")
                                            : qsTr("Retrieve receivers that heard your callsign during the last hour. PSK upload is independent.")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    Text {
                                        text: qsTr("PSK")
                                        color: "#ba7cff"
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                    ComboBox {
                                        Layout.preferredWidth: 96
                                        model: root.mapLayers
                                            ? root.mapLayers.availablePskDisplayModes
                                            : ["Overlay", "Replace"]
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(
                                                           root.mapLayers.pskDisplayMode))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.pskDisplayMode =
                                            currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Overlay PSK receivers with local activity or replace local activity")
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: 20
                                        to: 100
                                        stepSize: 5
                                        value: root.mapLayers
                                            ? root.mapLayers.pskOpacityPercent : 65
                                        onMoved: {
                                            if (root.mapLayers)
                                                root.mapLayers.pskOpacityPercent =
                                                    Math.round(value)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("PSK grid opacity")
                                    }
                                    Text {
                                        text: root.mapLayers
                                            ? root.mapLayers.pskOpacityPercent + "%" : "65%"
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    spacing: 5
                                    CheckBox {
                                        id: mqttEnabled
                                        text: qsTr("Live MQTT")
                                        checked: root.mapLayers
                                            && root.mapLayers.pskFeedService
                                            && root.mapLayers.pskFeedService.enabled
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (!root.mapLayers
                                                    || !root.mapLayers.pskFeedService)
                                                return
                                            if (checked)
                                                root.mapLayers.configurePskFeed(
                                                    root.engine.callsign,
                                                    root.engine.grid)
                                            else
                                                root.mapLayers.pskFeedService.enabled = false
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("The subscription is retained but paused while Offline mode is enabled")
                                            : qsTr("Subscribe to the PSK Reporter MQTT stream for this station")
                                    }
                                    Button {
                                        text: root.mapLayers && root.mapLayers.pskFeedService
                                              && root.mapLayers.pskFeedService.connected
                                            ? qsTr("Connected") : qsTr("Connect")
                                        font.pixelSize: 8
                                        enabled: root.mapLayers
                                            && root.mapLayers.pskFeedService
                                            && !root.mapLayerEnabled("offline")
                                        onClicked: root.mapLayers.configurePskFeed(
                                            root.engine.callsign, root.engine.grid)
                                        ToolTip.visible: hovered
                                        ToolTip.text: root.mapLayerEnabled("offline")
                                            ? qsTr("Unavailable while Offline mode is enabled")
                                            : qsTr("Connect the PSK Reporter MQTT feed")
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: root.mapLayers && root.mapLayers.pskFeedService
                                            ? root.mapLayers.pskFeedService.receivedCount
                                              + qsTr(" spots") : ""
                                        color: "#ba7cff"
                                        font.pixelSize: 8
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableSpotAgeFilters : []
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(root.mapLayers.spotAgeFilter)) : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.spotAgeFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("PSK/spot history age")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapLayers
                                            ? root.mapLayers.availableCorrelationFilters : []
                                        currentIndex: root.mapLayers
                                            ? Math.max(0, model.indexOf(root.mapLayers.spotCorrelationFilter)) : 0
                                        font.pixelSize: 9
                                        onActivated: root.mapLayers.spotCorrelationFilter = currentText
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Source correlation filter")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.pskLayerEnabled
                                    text: root.mapLayers
                                        ? qsTr("Heat %1  Timeline %2  Paths %3")
                                              .arg(root.mapLayers.spotHeatmap.length)
                                              .arg(root.mapLayers.spotTimeline.length)
                                              .arg(root.mapLayers.spotPaths.length)
                                        : ""
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: [qsTr("4-char grids"), qsTr("6-char grids")]
                                        currentIndex: root.mapLayers
                                            && root.mapLayers.gridPrecision === 6 ? 1 : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapLayers)
                                                root.mapLayers.gridPrecision =
                                                    currentIndex === 1 ? 6 : 4
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Coverage grid precision")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: [
                                            { label: qsTr("Live 5 min"), value: 5 },
                                            { label: qsTr("Live 15 min"), value: 15 },
                                            { label: qsTr("Live 30 min"), value: 30 },
                                            { label: qsTr("Live 60 min"), value: 60 }
                                        ]
                                        textRole: "label"
                                        valueRole: "value"
                                        currentIndex: {
                                            if (!root.mapLayers)
                                                return 1
                                            var values = [5, 15, 30, 60]
                                            var found = values.indexOf(
                                                root.mapLayers.liveDecayMinutes)
                                            return found >= 0 ? found : 1
                                        }
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapLayers)
                                                root.mapLayers.liveDecayMinutes =
                                                    Number(currentValue)
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Time before live grid activity fades out")
                                    }
                                }
                                CheckBox {
                                    text: qsTr("Split historical and live grid status")
                                    checked: root.mapLayers
                                        ? root.mapLayers.splitGridEnabled : true
                                    font.pixelSize: 9
                                    onToggled: {
                                        if (root.mapLayers)
                                            root.mapLayers.splitGridEnabled = checked
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show QSO/QSL history and current activity in separate halves of the same grid")
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    CheckBox {
                                        Layout.fillWidth: true
                                        text: qsTr("Push-pin live grids")
                                        checked: root.mapLayers
                                            ? root.mapLayers.coveragePushPinsEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.coveragePushPinsEnabled = checked
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Draw active and wanted grids as compact map pins")
                                    }
                                    CheckBox {
                                        Layout.fillWidth: true
                                        text: qsTr("UTC time zones")
                                        checked: root.mapLayers
                                            ? root.mapLayers.timeZoneOverlayEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapLayers)
                                                root.mapLayers.timeZoneOverlayEnabled = checked
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Show UTC-offset meridians on the map")
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 4
                                    columnSpacing: 5
                                    rowSpacing: 3
                                    Repeater {
                                        model: [
                                            { label: "QSO", color: "#22c7e8" },
                                            { label: "QSL", color: "#51e58a" },
                                            { label: "CQ", color: "#f6c344" },
                                            { label: "CQDX", color: "#ff8c42" },
                                            { label: "QRZ", color: "#ff8c42" },
                                            { label: "QSX", color: "#ba7cff" },
                                            { label: "WSPR", color: "#ba7cff" },
                                            { label: "PSK", color: "#ba7cff" }
                                        ]
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Rectangle {
                                                Layout.preferredWidth: 9
                                                Layout.preferredHeight: 9
                                                radius: 1
                                                color: modelData.color
                                            }
                                            Text {
                                                text: modelData.label
                                                color: root.textSecondary
                                                font.pixelSize: 8
                                            }
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.engine && root.engine.pskHeardByCount > 0
                                        ? qsTr("PSK Reporter: %1 receivers found")
                                              .arg(root.engine.pskHeardByCount)
                                        : qsTr("PSK Reporter: no receivers loaded for the last hour")
                                    color: root.engine && root.engine.pskHeardByCount > 0
                                        ? "#ba7cff" : root.textSecondary
                                    font.pixelSize: 9
                                    wrapMode: Text.Wrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("Confirmed = paper QSL, LoTW or eQSL received (ADIF status Y)")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    Text {
                                        text: qsTr("QSO  %1").arg(root.mapLayers
                                                                  ? root.mapLayers.qsoCount : 0)
                                        color: root.textSecondary; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Live  %1").arg(root.mapLayers
                                                                   ? root.mapLayers.liveSpotCount : 0)
                                        color: root.textSecondary; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Worked  %1").arg(root.mapLayers
                                                                     ? root.mapLayers.workedGridCount : 0)
                                        color: root.secondaryCyan; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Confirmed  %1").arg(root.mapLayers
                                                                        ? root.mapLayers.confirmedGridCount : 0)
                                        color: root.accentGreen; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Active  %1").arg(root.mapLayers
                                                                    ? root.mapLayers.activeGridCount : 0)
                                        color: "#f6c344"; font.pixelSize: 9
                                    }
                                    Text {
                                        text: qsTr("Missing  %1").arg(root.mapLayers
                                                                     ? root.mapLayers.missingGridCount : 0)
                                        color: "#ff8c42"; font.pixelSize: 9
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel.layerEnabled("moon")
                                    spacing: 5

                                    Text {
                                        Layout.fillWidth: true
                                        text: root.externalOverlays
                                              && root.externalOverlays.moonDataAvailable
                                            ? qsTr("Moon  Az %1°  El %2°  ·  Subpoint %3 %4")
                                                  .arg(root.externalOverlays.moonAzimuth.toFixed(1))
                                                  .arg(root.externalOverlays.moonElevation.toFixed(1))
                                                  .arg(root.coordinateText(
                                                           root.externalOverlays.moonSublunarLatitude,
                                                           "N", "S"))
                                                  .arg(root.coordinateText(
                                                           root.externalOverlays.moonSublunarLongitude,
                                                           "E", "W"))
                                            : qsTr("Moon data unavailable")
                                        color: root.externalOverlays
                                               && root.externalOverlays.moonDataAvailable
                                            ? "#dbe7ff" : root.textSecondary
                                        font.pixelSize: 9
                                        wrapMode: Text.Wrap
                                    }
                                    Button {
                                        visible: root.externalOverlays
                                            && root.externalOverlays.moonDataAvailable
                                        text: qsTr("LOCATE")
                                        font.pixelSize: 8
                                        Layout.preferredHeight: 24
                                        onClicked: root.locateMoon()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Center map on the Moon subpoint")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel.layerEnabled("propagation")
                                    text: root.engine && root.engine.propagationManager
                                        ? qsTr("Propagation  SFI %1  K %2  MUF %3")
                                              .arg(root.engine.propagationManager.solarFlux)
                                              .arg(root.engine.propagationManager.kIndex)
                                              .arg(root.engine.propagationManager.muf)
                                        : qsTr("Propagation data unavailable")
                                    color: "#ffcf66"
                                    font.pixelSize: 9
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: root.mapLayers
                                        && root.mapLayers.layerModel
                                           && root.mapLayers.layerModel
                                                  .layerEnabled("propagation")
                                    text: qsTr("Propagation controls MUF, foF2, Es and Aurora")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    wrapMode: Text.Wrap
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: root.mapLayerEnabled("muf")
                                        || root.mapLayerEnabled("fof2")
                                        || root.mapLayerEnabled("es")
                                    spacing: 3

                                    Text {
                                        text: qsTr("PROPAGATION SCALE")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: qsTr("MUF and foF2 use MHz. Es is a probability index, not a frequency.")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        wrapMode: Text.Wrap
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("muf")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: "MUF"
                                                color: "#f6c344"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#b89fbe"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#aadbd1"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#fdf6ab"
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("<5 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("14 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr(">28 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("fof2")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: "foF2"
                                                color: "#66d9ff"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#b89fbe"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#aadbd1"
                                                }
                                                Rectangle {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: 8
                                                    color: "#fdf6ab"
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("<1 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("5 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr(">14 MHz"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: root.mapLayerEnabled("es")
                                        spacing: 1
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Text {
                                                Layout.preferredWidth: 34
                                                text: "Es"
                                                color: "#ff9f43"
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 0
                                                Repeater {
                                                    model: ["#728890", "#39ddce", "#4d9817",
                                                            "#f7f500", "#fbb800", "#db2d01",
                                                            "#7030a0", "#f2f2f2", "#5c5c61"]
                                                    delegate: Rectangle {
                                                        required property string modelData
                                                        Layout.fillWidth: true
                                                        Layout.preferredHeight: 8
                                                        color: modelData
                                                    }
                                                }
                                            }
                                        }
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Item { Layout.preferredWidth: 39 }
                                            Text { text: qsTr("Poor"); color: root.textSecondary; font.pixelSize: 8 }
                                            Item { Layout.fillWidth: true }
                                            Text { text: qsTr("Good"); color: root.textSecondary; font.pixelSize: 8 }
                                        }
                                    }
                                }
                                Text {
                                    text: qsTr("MAP OPERATIONS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.availableDataViews : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.dataViewMode))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.dataViewMode =
                                                    currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Live and logbook visibility")
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.availableProjections : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.mapProjection))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.mapProjection =
                                                    currentText
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Projection")
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: root.mapOperations
                                            ? root.mapOperations.mapPresets : []
                                        currentIndex: root.mapOperations
                                            ? Math.max(0, model.indexOf(
                                                           root.mapOperations.activeMapPreset))
                                            : 0
                                        font.pixelSize: 9
                                        onActivated: {
                                            if (root.mapOperations)
                                                root.mapOperations.applyMapPreset(
                                                    currentText)
                                        }
                                    }
                                    Button {
                                        text: qsTr("SCREENSHOT")
                                        font.pixelSize: 8
                                        onClicked: root.captureMapScreenshot()
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    CheckBox {
                                        text: "PSTRotator"
                                        checked: root.mapOperations
                                            ? root.mapOperations.rotatorEnabled : false
                                        font.pixelSize: 9
                                        onToggled: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorEnabled =
                                                    checked
                                        }
                                    }
                                    TextField {
                                        Layout.fillWidth: true
                                        text: root.mapOperations
                                            ? root.mapOperations.rotatorHost : ""
                                        placeholderText: "127.0.0.1"
                                        font.pixelSize: 9
                                        onEditingFinished: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorHost = text
                                        }
                                    }
                                    SpinBox {
                                        Layout.preferredWidth: 88
                                        from: 1
                                        to: 65535
                                        editable: true
                                        value: root.mapOperations
                                            ? root.mapOperations.rotatorPort : 12040
                                        font.pixelSize: 8
                                        onValueModified: {
                                            if (root.mapOperations)
                                                root.mapOperations.rotatorPort = value
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.mapOperations
                                        ? root.mapOperations.rotatorStatus : ""
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: Qt.rgba(root.secondaryCyan.r,
                                                   root.secondaryCyan.g,
                                                   root.secondaryCyan.b, 0.22)
                                    visible: root.externalOverlays
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.externalOverlays
                                    Text {
                                        text: qsTr("EXTERNAL DATA")
                                        color: root.secondaryCyan
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    ToolButton {
                                        Layout.preferredHeight: 24
                                        text: qsTr("Refresh")
                                        font.pixelSize: 9
                                        enabled: root.externalOverlays
                                                 && !root.externalOverlays.loading
                                        onClicked: root.externalOverlays.refreshAll()
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Refresh enabled external layers")
                                    }
                                }
                                Repeater {
                                    model: root.externalOverlays
                                        ? root.externalOverlays.providerStatus : []
                                    delegate: ColumnLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 1
                                        visible: modelData.enabled
                                        Text {
                                            Layout.fillWidth: true
                                            text: (modelData.loading ? "↻ " : "")
                                                  + modelData.label + "  "
                                                  + (modelData.available
                                                     ? root.overlayUpdatedText(
                                                           modelData.updatedMs)
                                                     : qsTr("unavailable"))
                                            color: modelData.error
                                                ? "#ff8c42" : root.textPrimary
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.error
                                                ? modelData.error
                                                : (modelData.attributionUrl
                                                   ? qsTr("Source: <a href=\"%1\">%2</a>")
                                                         .arg(modelData.attributionUrl)
                                                         .arg(modelData.attribution)
                                                   : qsTr("Source: %1")
                                                         .arg(modelData.attribution))
                                            textFormat: modelData.attributionUrl
                                                ? Text.RichText : Text.PlainText
                                            color: root.textSecondary
                                            linkColor: root.secondaryCyan
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                            onLinkActivated: function(link) {
                                                Qt.openUrlExternally(link)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ScrollView {
                            id: rosterScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: rosterContent
                                width: rosterScroll.availableWidth
                                spacing: 4
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 4
                                rowSpacing: 0
                                CheckBox {
                                    text: qsTr("New grid")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertNewGridEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertNewGridEnabled =
                                        checked
                                }
                                CheckBox {
                                    text: qsTr("New DXCC")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertNewDxccEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertNewDxccEnabled =
                                        checked
                                }
                                CheckBox {
                                    text: qsTr("CQ activity")
                                    checked: root.mapLayers
                                        ? root.mapLayers.alertCqEnabled : true
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.alertCqEnabled =
                                        checked
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Call pattern, e.g. 9H*")
                                    text: root.mapLayers
                                        ? root.mapLayers.alertCallPattern : ""
                                    font.pixelSize: 9
                                    selectByMouse: true
                                    onEditingFinished: root.mapLayers
                                        .alertCallPattern = text
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Wildcard pattern matched against callsigns and messages")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                ComboBox {
                                    Layout.preferredWidth: 88
                                    model: [
                                        qsTr("No filter"), qsTr("Only"),
                                        qsTr("Exclude"), qsTr("Regex")
                                    ]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterTextMode)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterTextMode = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Call roster text-filter mode")
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("Call, grid, DXCC or message")
                                    text: root.mapLayers
                                        ? root.mapLayers.rosterTextFilter : ""
                                    font.pixelSize: 9
                                    selectByMouse: true
                                    onEditingFinished: {
                                        if (root.mapLayers)
                                            root.mapLayers.rosterTextFilter = text
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Filtering runs in the map database worker")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: qsTr("LOOKUP")
                                    color: root.secondaryCyan
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableCallLookupProviders
                                        : ["QRZ", "HamQTH", "QRZCQ"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.callLookupProvider))
                                        : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.callLookupProvider =
                                        currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Provider used by lookup actions in grid popups and the roster")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text {
                                    text: qsTr("AWARD")
                                    color: root.secondaryCyan
                                    font.pixelSize: 8
                                    font.bold: true
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardPrograms : ["None"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.activeAwardProgram)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.activeAwardProgram = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Select the award whose missing entities become wanted stations")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 92
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardGoals : ["Confirmed"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardGoal)) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.awardGoal = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Hunt entities not yet worked or not yet confirmed")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterStatuses : ["All"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterStatusFilter)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterStatusFilter = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show all, new, unconfirmed, wanted or watched stations")
                                }
                                CheckBox {
                                    text: qsTr("CQ")
                                    checked: root.mapLayers ? root.mapLayers.rosterCqOnly : false
                                    font.pixelSize: 9
                                    onToggled: root.mapLayers.rosterCqOnly = checked
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Show only active CQ calls in the roster")
                                }
                                ToolButton {
                                    text: qsTr("Clear")
                                    enabled: root.mapLayers && root.mapLayers.liveSpotCount > 0
                                    onClicked: root.mapLayers.clearLiveSpots()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Clear live station history")
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        ? qsTr("Lists %1").arg(
                                              root.mapLayers.rosterPreferenceCount)
                                        : qsTr("Lists")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterPreferences =
                                        !root.showRosterPreferences
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Review watched calls and ignored calls or DXCC entities")
                                }
                                ToolButton {
                                    text: qsTr("Columns")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterColumns = !root.showRosterColumns
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Choose the information shown on each roster row")
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        ? qsTr("Rules %1").arg(root.mapLayers.rosterRules.length)
                                        : qsTr("Rules")
                                    enabled: !!root.mapLayers
                                    onClicked: root.showRosterRules = !root.showRosterRules
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Create wanted, ignored or watched rules with optional band and mode scopes")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableRosterHuntScopes : ["All time"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.rosterHuntScope)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterHuntScope = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Compare worked and confirmed status globally, by band or by band and mode")
                                }
                                SpinBox {
                                    id: rosterRetention
                                    from: 1
                                    to: 60
                                    editable: true
                                    value: root.mapLayers
                                        ? root.mapLayers.rosterRetentionMinutes : 5
                                    Layout.preferredWidth: 82
                                    font.pixelSize: 9
                                    textFromValue: function(value, locale) {
                                        return value + " min"
                                    }
                                    valueFromText: function(text, locale) {
                                        var parsed = parseInt(text)
                                        return isNaN(parsed) ? 5 : parsed
                                    }
                                    onValueModified: {
                                        if (root.mapLayers)
                                            root.mapLayers.rosterRetentionMinutes = value
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Keep each station active in the roster for this many minutes")
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: ["Time", "Call", "Grid", "SNR", "Distance", "DXCC"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(root.mapLayers.rosterSort)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.rosterSort = currentText
                                }
                                ToolButton {
                                    text: root.mapLayers
                                        && root.mapLayers.rosterSortDescending ? "↓" : "↑"
                                    onClicked: root.mapLayers.rosterSortDescending =
                                        !root.mapLayers.rosterSortDescending
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Reverse sort")
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.mapLayers
                                    ? qsTr("%1 stations · %2 wanted · %3 new · %4 unconfirmed")
                                          .arg(root.mapLayers.rosterCount)
                                          .arg(root.mapLayers.rosterWantedCount)
                                          .arg(root.mapLayers.rosterNewCount)
                                          .arg(root.mapLayers.rosterUnconfirmedCount)
                                    : ""
                                color: root.textSecondary
                                font.pixelSize: 8
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 96 : 0
                                visible: root.showRosterColumns && !!root.mapLayers
                                clip: true
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 3
                                    Text {
                                        text: qsTr("VISIBLE ROSTER COLUMNS")
                                        color: root.secondaryCyan
                                        font.pixelSize: 8
                                        font.bold: true
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Repeater {
                                            model: root.mapLayers
                                                ? root.mapLayers.availableRosterColumns : []
                                            delegate: CheckBox {
                                                required property string modelData
                                                text: modelData
                                                font.pixelSize: 8
                                                checked: root.mapLayers
                                                    && root.mapLayers.rosterVisibleColumns.indexOf(modelData) >= 0
                                                onToggled: {
                                                    if (!root.mapLayers)
                                                        return
                                                    var columns = root.mapLayers.rosterVisibleColumns.slice()
                                                    var columnIndex = columns.indexOf(modelData)
                                                    if (checked && columnIndex < 0)
                                                        columns.push(modelData)
                                                    else if (!checked && columnIndex >= 0)
                                                        columns.splice(columnIndex, 1)
                                                    root.mapLayers.rosterVisibleColumns = columns
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: visible ? 156 : 0
                                visible: root.showRosterRules && !!root.mapLayers
                                clip: true
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    spacing: 3
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("OPERATIONAL RULES")
                                            color: root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        ToolButton {
                                            text: qsTr("Hide")
                                            font.pixelSize: 8
                                            onClicked: root.showRosterRules = false
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: rosterRuleType
                                            Layout.preferredWidth: 86
                                            model: ["CALL", "GRID", "DXCC", "WPX", "CQ", "ITU",
                                                    "STATE", "CONTINENT", "COUNTY", "POTA", "IOTA",
                                                    "OQRS", "BAND", "MODE"]
                                            font.pixelSize: 8
                                        }
                                        TextField {
                                            id: rosterRuleValue
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Value")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                        ComboBox {
                                            id: rosterRuleAction
                                            Layout.preferredWidth: 74
                                            model: ["WANTED", "WATCH", "IGNORE"]
                                            font.pixelSize: 8
                                        }
                                        Button {
                                            text: qsTr("Add")
                                            font.pixelSize: 8
                                            enabled: root.mapLayers && rosterRuleValue.text.trim().length > 0
                                            onClicked: {
                                                root.mapLayers.setRosterRule(
                                                    rosterRuleType.currentText,
                                                    rosterRuleValue.text,
                                                    rosterRuleAction.currentText,
                                                    rosterRuleBand.text,
                                                    rosterRuleMode.text)
                                                rosterRuleValue.clear()
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            id: rosterRuleBand
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Band scope, optional")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                        TextField {
                                            id: rosterRuleMode
                                            Layout.fillWidth: true
                                            placeholderText: qsTr("Mode scope, optional")
                                            font.pixelSize: 8
                                            selectByMouse: true
                                        }
                                    }
                                    ListView {
                                        id: rosterRuleList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        spacing: 1
                                        model: root.mapLayers ? root.mapLayers.rosterRules : []
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                        delegate: RowLayout {
                                            required property var modelData
                                            width: rosterRuleList.width
                                            height: 22
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.action + "  " + modelData.type + "="
                                                    + modelData.value
                                                    + (modelData.band ? "  " + modelData.band : "")
                                                    + (modelData.mode ? "  " + modelData.mode : "")
                                                color: modelData.action === "IGNORE"
                                                    ? "#ff7b7b" : (modelData.action === "WATCH"
                                                        ? root.secondaryCyan : root.accentGreen)
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                            ToolButton {
                                                Layout.preferredWidth: 24
                                                text: "×"
                                                font.pixelSize: 9
                                                onClicked: root.mapLayers.removeRosterRule(
                                                    modelData.type, modelData.value,
                                                    modelData.band || "", modelData.mode || "")
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Remove rule")
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(
                                    156, 34 + (root.mapLayers
                                              ? root.mapLayers.rosterPreferenceCount
                                                * 29 : 0))
                                visible: root.showRosterPreferences
                                color: "#101a28"
                                border.width: 1
                                border.color: root.glassBorder
                                radius: 3

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    spacing: 2
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: qsTr("WATCHED / IGNORED LISTS")
                                            color: root.secondaryCyan
                                            font.pixelSize: 8
                                            font.bold: true
                                        }
                                        ToolButton {
                                            text: qsTr("Clear all")
                                            font.pixelSize: 8
                                            enabled: root.mapLayers
                                                && root.mapLayers.rosterPreferenceCount > 0
                                            onClicked: root.mapLayers
                                                .clearRosterPreferences()
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Clear all watched calls and all call or DXCC exclusions")
                                        }
                                    }
                                    ListView {
                                        id: rosterPreferenceList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        spacing: 1
                                        model: root.mapLayers
                                            ? root.mapLayers.rosterPreferences : []
                                        ScrollBar.vertical: ScrollBar {
                                            policy: ScrollBar.AsNeeded
                                        }
                                        delegate: RowLayout {
                                            required property var modelData
                                            width: rosterPreferenceList.width
                                            height: 27
                                            spacing: 4
                                            Rectangle {
                                                Layout.preferredWidth: 48
                                                Layout.preferredHeight: 20
                                                radius: 3
                                                color: modelData.type === "WATCH"
                                                    ? "#182538" : "#2d2513"
                                                border.width: 1
                                                border.color: modelData.type === "WATCH"
                                                    ? root.secondaryCyan : "#f6c344"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.type
                                                    color: parent.border.color
                                                    font.pixelSize: 8
                                                    font.bold: true
                                                }
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.value
                                                color: root.textPrimary
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                            ToolButton {
                                                Layout.preferredWidth: 24
                                                text: "×"
                                                onClicked: root.mapLayers
                                                    .removeRosterPreference(
                                                        modelData.type,
                                                        modelData.value)
                                                ToolTip.visible: hovered
                                                ToolTip.text: qsTr("Remove this entry")
                                            }
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: root.mapLayers
                                            && root.mapLayers.rosterPreferenceCount === 0
                                        text: qsTr("No watched or ignored entries")
                                        color: root.textSecondary
                                        font.pixelSize: 8
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                            ListView {
                                id: rosterList
                                Layout.fillWidth: true
                                // The roster controls can be taller than a docked map.
                                // Keep the station list independently scrollable while the
                                // enclosing ScrollView makes every control reachable.
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(360, intelligencePanel.height * 0.45))
                                clip: true
                                spacing: 3
                                model: root.mapLayers ? root.mapLayers.roster : []
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: rosterList.width
                                    height: 70
                                    radius: 3
                                    color: modelData.watched
                                        ? "#182538"
                                        : modelData.wanted
                                        ? root.rosterStatusFill(modelData.status)
                                        : (index % 2 ? "#101a28" : "#0d2430")
                                    border.width: modelData.wanted || modelData.watched ? 1 : 0
                                    border.color: modelData.watched
                                        ? root.secondaryCyan
                                        : modelData.wanted
                                        ? root.rosterStatusColor(modelData.status)
                                        : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1
                                            Text {
                                                Layout.fillWidth: true
                                                text: (modelData.call || qsTr("Unknown"))
                                                      + (modelData.grid ? "  " + modelData.grid : "")
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 11
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                text: root.rosterColumnSummary(modelData)
                                                color: root.textSecondary
                                                font.pixelSize: 9
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: !!modelData.huntReason
                                                text: modelData.huntReason || ""
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                            }
                                        }
                                        Rectangle {
                                            Layout.preferredWidth: rosterStatusLabel.implicitWidth + 12
                                            Layout.preferredHeight: 22
                                            radius: 3
                                            color: root.rosterStatusFill(modelData.status)
                                            border.width: 1
                                            border.color: root.rosterStatusColor(modelData.status)
                                            Text {
                                                id: rosterStatusLabel
                                                anchors.centerIn: parent
                                                text: modelData.status || "LIVE"
                                                color: root.rosterStatusColor(modelData.status)
                                                font.pixelSize: 8
                                                font.bold: true
                                            }
                                        }
                                        ToolButton {
                                            text: modelData.watched ? "★" : "☆"
                                            font.pixelSize: 13
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallWatched(
                                                modelData.call, !modelData.watched)
                                            ToolTip.visible: hovered
                                            ToolTip.text: modelData.watched
                                                ? qsTr("Stop watching this station")
                                                : qsTr("Keep this station at the top of the roster")
                                        }
                                        ToolButton {
                                            text: "X"
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.mapLayers.setRosterCallIgnored(
                                                modelData.call, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Ignore this station until roster preferences are reset")
                                        }
                                        ToolButton {
                                            text: qsTr("DX")
                                            font.pixelSize: 8
                                            enabled: !!modelData.dxcc
                                            visible: !!modelData.dxcc
                                            onClicked: root.mapLayers.setRosterDxccIgnored(
                                                modelData.dxcc, true)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Ignore every station from %1")
                                                .arg(modelData.dxcc || "")
                                        }
                                        ToolButton {
                                            text: "QRZ"
                                            font.pixelSize: 8
                                            enabled: !!modelData.call
                                            onClicked: root.openCallLookup(modelData.call)
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Open callsign lookup")
                                        }
                                        ToolButton {
                                            text: qsTr("CALL")
                                            font.pixelSize: 9
                                            enabled: !!modelData.call
                                            onClicked: root.engine.processMapRosterCall(
                                                modelData.call, modelData.grid || "")
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Start QSO with this station")
                                        }
                                    }
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }

                        MapLogbookPanel {
                            operations: root.mapOperations
                            borderColor: root.glassBorder
                            primaryColor: root.primaryBlue
                            accentColor: root.accentGreen
                            textColor: root.textPrimary
                            mutedColor: root.textSecondary
                            onCallRequested: function(call, grid) {
                                if (root.engine)
                                    root.engine.processMapRosterCall(call, grid)
                            }
                        }

                        ScrollView {
                            id: statisticsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: statisticsContent
                                width: statisticsScroll.availableWidth
                                spacing: 6
                                property var stats: root.mapLayers
                                    ? root.mapLayers.statistics : ({})

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            var period = statisticsContent.stats.period || "All time"
                                            var filtered = Number(statisticsContent.stats.qso || 0)
                                            var total = Number(statisticsContent.stats.totalQso || 0)
                                            return period === "All time"
                                                ? qsTr("LOGBOOK · ALL TIME · %1 QSO").arg(total)
                                                : qsTr("FILTERED · %1 · %2 of %3 QSO")
                                                      .arg(period).arg(filtered).arg(total)
                                        }
                                        color: (statisticsContent.stats.period || "All time") === "All time"
                                            ? root.secondaryCyan : root.accentAmber
                                        font.pixelSize: 9
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    ToolButton {
                                        visible: root.mapLayers
                                                 && root.mapLayers.periodFilter !== "All time"
                                        text: qsTr("ALL TIME")
                                        font.pixelSize: 8
                                        onClicked: root.mapLayers.periodFilter = "All time"
                                        ToolTip.visible: hovered
                                        ToolTip.text: qsTr("Remove the temporary map time filter")
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.mapLayers && root.mapLayers.sourcePath
                                        ? qsTr("ADIF  %1").arg(root.mapLayers.sourcePath)
                                        : qsTr("No active ADIF logbook")
                                    color: root.textSecondary
                                    font.pixelSize: 8
                                    elide: Text.ElideMiddle
                                    ToolTip.visible: sourcePathMouse.containsMouse
                                    ToolTip.text: text
                                    MouseArea {
                                        id: sourcePathMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 8
                                    rowSpacing: 5
                                    Repeater {
                                        model: [
                                            { label: qsTr("QSO"), value: statisticsContent.stats.qso || 0 },
                                            { label: qsTr("Confirmed"), value: statisticsContent.stats.confirmed || 0 },
                                            { label: qsTr("Calls"), value: statisticsContent.stats.calls || 0 },
                                            { label: qsTr("DXCC"), value: statisticsContent.stats.dxcc || 0 },
                                            { label: qsTr("Grids"), value: statisticsContent.stats.grids || 0 },
                                            { label: qsTr("Live spots"), value: statisticsContent.stats.live || 0 }
                                        ]
                                        delegate: Rectangle {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 40
                                            radius: 3
                                            color: "#101a28"
                                            border.width: 1
                                            border.color: root.glassBorder
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 1
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.value
                                                    color: root.secondaryCyan
                                                    font.pixelSize: 13
                                                    font.bold: true
                                                }
                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.label
                                                    color: root.textSecondary
                                                    font.pixelSize: 8
                                                }
                                            }
                                        }
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("ADIF span  %1 — %2")
                                        .arg(root.statisticsDate(
                                            statisticsContent.stats.totalFirstEpoch))
                                        .arg(root.statisticsDate(
                                            statisticsContent.stats.totalLastEpoch))
                                    color: root.textSecondary
                                    font.pixelSize: 9
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: Number(statisticsContent.stats.qso || 0) === 0
                                             && Number(statisticsContent.stats.totalQso || 0) > 0
                                    text: qsTr("The ADIF logbook is loaded, but no QSO matches the current map filters.")
                                    color: root.accentAmber
                                    font.pixelSize: 8
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    text: qsTr("TOP BANDS")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Repeater {
                                    model: statisticsContent.stats.bands || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: root.textPrimary
                                            font.pixelSize: 9
                                        }
                                        Text {
                                            text: qsTr("%1 QSO · %2 QSL")
                                                .arg(modelData.qso)
                                                .arg(modelData.confirmed)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 1
                                    color: root.glassBorder
                                }
                                Text {
                                    text: qsTr("TOP MODES")
                                    color: root.secondaryCyan
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                                Repeater {
                                    model: statisticsContent.stats.modes || []
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            color: root.textPrimary
                                            font.pixelSize: 9
                                        }
                                        Text {
                                            text: qsTr("%1 QSO · %2 QSL")
                                                .arg(modelData.qso)
                                                .arg(modelData.confirmed)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                            }
                        }

                        ScrollView {
                            id: awardsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: awardsContent
                                width: awardsScroll.availableWidth
                                spacing: 5
                            RowLayout {
                                Layout.fillWidth: true
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardPrograms : ["None"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.activeAwardProgram)) : 0
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.activeAwardProgram = currentText
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Use this award to identify wanted stations in the roster")
                                }
                                ComboBox {
                                    Layout.preferredWidth: 96
                                    model: root.mapLayers
                                        ? root.mapLayers.availableAwardGoals : ["Confirmed"]
                                    currentIndex: root.mapLayers
                                        ? Math.max(0, model.indexOf(
                                                       root.mapLayers.awardGoal)) : 0
                                    enabled: root.mapLayers
                                        && root.mapLayers.activeAwardProgram !== "None"
                                    font.pixelSize: 9
                                    onActivated: root.mapLayers.awardGoal = currentText
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: root.mapLayers
                                    && root.mapLayers.activeAwardProgram !== "None"
                                    ? qsTr("%1 / %2 drives the Award and Wanted roster filters")
                                          .arg(root.mapLayers.activeAwardProgram)
                                          .arg(root.mapLayers.awardGoal)
                                    : qsTr("Select an award to make its missing entities operational targets")
                                color: root.textSecondary
                                font.pixelSize: 8
                                wrapMode: Text.Wrap
                            }
                            ListView {
                                id: awardsList
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(420, intelligencePanel.height * 0.62))
                                clip: true
                                spacing: 5
                                model: root.mapLayers ? root.mapLayers.awards : []
                                ScrollBar.vertical: ScrollBar {
                                    policy: ScrollBar.AsNeeded
                                }
                                delegate: Rectangle {
                                    required property var modelData
                                    width: awardsList.width
                                    height: 74
                                    radius: 3
                                    color: modelData.selected
                                        ? "#182538" : "#101a28"
                                    border.width: modelData.selected ? 1 : 0
                                    border.color: root.secondaryCyan
                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        spacing: 2
                                        Text {
                                            text: qsTr("%1  %2 / %3")
                                                .arg(modelData.label)
                                                .arg(modelData.achieved)
                                                .arg(modelData.target)
                                            color: modelData.complete
                                                ? root.accentGreen
                                                : root.textPrimary
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                        ProgressBar {
                                            width: parent.width
                                            from: 0
                                            to: 1
                                            value: modelData.progress
                                        }
                                        Text {
                                            width: parent.width
                                            text: qsTr("Worked %1 · Confirmed %2 · Remaining %3")
                                                .arg(modelData.worked)
                                                .arg(modelData.confirmed)
                                                .arg(modelData.remaining)
                                            color: root.textSecondary
                                            font.pixelSize: 8
                                            elide: Text.ElideRight
                                        }
                                    }
                                    MouseArea {
                                        id: awardMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.mapLayers.activeAwardProgram =
                                            modelData.label
                                    }
                                    ToolTip.visible: awardMouse.containsMouse
                                    ToolTip.text: (modelData.rule || "")
                                        + "\n"
                                        + qsTr("Select %1 as the active roster award")
                                              .arg(modelData.label)
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }

                        ScrollView {
                            id: alertsScroll
                            clip: true
                            contentWidth: availableWidth
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: alertsContent
                                width: alertsScroll.availableWidth
                                spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: qsTr("Mark read")
                                    font.pixelSize: 9
                                    enabled: root.mapLayers
                                        && root.mapLayers.unreadAlertCount > 0
                                    onClicked: root.mapLayers.markAlertsRead()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: qsTr("Clear")
                                    font.pixelSize: 9
                                    onClicked: root.mapLayers.clearAlerts()
                                }
                            }
                            ListView {
                                id: alertsList
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.max(
                                    180, Math.min(420, intelligencePanel.height * 0.68))
                                clip: true
                                spacing: 3
                                model: root.mapLayers ? root.mapLayers.alerts : []
                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                                delegate: Rectangle {
                                    required property var modelData
                                    required property int index
                                    width: alertsList.width
                                    height: 48
                                    radius: 3
                                    color: modelData.read ? "#101a28" : "#2d2513"
                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 6
                                        wrapMode: Text.Wrap
                                        text: modelData.message
                                        color: modelData.read
                                            ? root.textSecondary : "#f6c344"
                                        font.pixelSize: 9
                                    }
                                }
                            }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 8
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    MapOperationsWindows {
        id: mapOperationsWindows
        engine: root.engine
        mapLayers: root.mapLayers
        operations: root.mapOperations
        externalOverlays: root.externalOverlays
        backgroundColor: root.bgDeep
        borderColor: root.glassBorder
        primaryColor: root.primaryBlue
        accentColor: root.accentGreen
        textColor: root.textPrimary
        mutedColor: root.textSecondary
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        enabled: root.visible && root.mapOperations
        onActivated: root.mapOperations.cycleDataView()
    }
    Shortcut {
        sequence: "Ctrl+Shift+S"
        enabled: root.visible && root.mapOperations
        onActivated: root.captureMapScreenshot()
    }
    Shortcut {
        sequence: "Ctrl+Shift+R"
        enabled: root.visible
        onActivated: mapOperationsWindows.openRoster()
    }
    Shortcut {
        sequence: "Ctrl+Shift+T"
        enabled: root.visible
        onActivated: mapOperationsWindows.openStatistics()
    }
    Shortcut {
        sequence: "Ctrl+Shift+C"
        enabled: root.visible
        onActivated: mapOperationsWindows.openConditions()
    }

    Connections {
        target: engine
        ignoreUnknownSignals: true

        function onGridChanged() {
            root.syncMapSettings()
            root.updateMoonOverlay()
            if (root.visible)
                root.scheduleRebuild()
        }
        // 1.0.209 — RIMOSSO: onDecodeListChanged + onRxDecodeListChanged
        // triggeravano scheduleRebuild() = clearContacts + replayWorldMapFeed
        // a ogni decode (2/sec). I nuovi contact arrivano gia' incrementali
        // via onWorldMapContactAdded signal del bridge, no replay full
        // necessario.
        //
        // I signal TX/QSO sotto chiamano solo syncTxState() (cambio target
        // line lampeggiante, no rebuild): leggera e safe a ogni cambio.
        function onTransmittingChanged() {
            root.syncTxState()
        }
        function onTuningChanged() {
            root.syncTxState()
        }
        function onDxCallChanged() {
            root.syncTxState()
        }
        function onDxGridChanged() {
            root.syncTxState()
        }
        function onCurrentTxChanged() {
            root.syncTxState()
        }
        function onTxEnabledChanged() {
            root.syncTxState()
        }
        function onQsoProgressChanged() {
            root.syncTxState()
        }
        function onAutoCqRepeatChanged() {
            root.syncTxState()
        }
        function onModeChanged() {
            root.syncTxState()
        }
        function onSettingValueChanged(key, value) {
            if (key === "LiveMapUseGpu") {
                root.gpuLiveMapEnabled = !!value
                return
            }
            if (!worldMap)
                return
            if (key === "ShowGreyline" || key === "MapShowGreyline") {
                worldMap.setGreylineEnabled(!!value)
            } else if (key === "Miles") {
                worldMap.setDistanceInMiles(root.coerceBool(value, false))
            } else if (key === "WorldMapDisplayed" && root.visible) {
                root.scheduleRebuild()
            }
        }
        function onWorldMapResetRequested() {
            if (!root.visible || !worldMap)
                return
            worldMap.clearContacts()
        }
        function onWorldMapContactAdded(call, sourceGrid, destinationGrid, role) {
            if (!root.visible || !worldMap
                    || (root.mapLayers && !root.mapLayers.liveLayerEnabled)
                    || !root.decoderFeedAllowed())
                return
            worldMap.addContact(call, sourceGrid, destinationGrid, role)
        }
        function onWorldMapContactAddedByLonLat(call, sourceLon, sourceLat, destinationGrid, role) {
            if (!root.visible || !worldMap
                    || (root.mapLayers && !root.mapLayers.liveLayerEnabled)
                    || !root.decoderFeedAllowed())
                return
            worldMap.addContactByLonLat(call, sourceLon, sourceLat, destinationGrid, role)
        }
        function onWorldMapContactDowngraded(call) {
            if (!root.visible || !worldMap || !root.decoderFeedAllowed())
                return
            worldMap.downgradeContactToBand(call)
        }
    }

    Connections {
        target: root.mapLayers
        ignoreUnknownSignals: true

        function onCoverageChanged() {
            root.syncCoverage()
        }
        function onFiltersChanged() {
            if (root.visible)
                root.scheduleRebuild()
        }
        function onLiveLayerEnabledChanged() {
            if (!root.worldMap)
                return
            root.worldMap.clearContacts()
            if (root.visible && root.mapLayers.liveLayerEnabled
                    && root.decoderFeedAllowed())
                root.engine.replayWorldMapFeed()
            root.syncSpotPaths()
        }
        function onSpotAnalyticsChanged() {
            if (root.visible && root.mapLayers && root.mapLayers.pskLayerEnabled)
                spotPathRefreshTimer.restart()
        }
        function onCoveragePushPinsEnabledChanged() {
            root.syncMapSettings()
        }
        function onTimeZoneOverlayEnabledChanged() {
            root.syncMapSettings()
        }
    }

    Timer {
        id: spotPathRefreshTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (!root.visible || !root.worldMap)
                return
            root.worldMap.clearContacts()
            if ((!root.mapLayers || root.mapLayers.liveLayerEnabled)
                    && root.decoderFeedAllowed())
                root.engine.replayWorldMapFeed()
            root.syncSpotPaths()
            root.syncTxState()
        }
    }

    Connections {
        target: root.mapOperations
        ignoreUnknownSignals: true

        function onOperationalMarkersChanged() {
            root.syncOperations()
        }
        function onGeographicFeaturesChanged() {
            root.syncOperations()
        }
        function onMapProjectionChanged() {
            root.syncOperations()
        }
        function onSelectedPotaParkChanged() {
            if (root.operationalDetailsVisible
                    && root.mapOperations.selectedPotaPark
                    && Object.keys(root.mapOperations.selectedPotaPark).length > 0)
                root.selectedOperationalDetails =
                    root.mapOperations.selectedPotaPark
        }
    }

    Connections {
        target: root.externalOverlays
        ignoreUnknownSignals: true

        function onEarthquakeFeaturesChanged() {
            root.syncOperations()
        }
        function onMoonDataChanged() {
            root.syncOperations()
            if (root.moonLocatePending && root.externalOverlays
                    && root.externalOverlays.moonDataAvailable) {
                root.moonLocatePending = false
                Qt.callLater(root.locateMoon)
            }
        }
    }

    Connections {
        target: root.mapLayers ? root.mapLayers.layerModel : null
        ignoreUnknownSignals: true

        function onLayerToggled(layerId, enabled) {
            if (enabled && (layerId === "states" || layerId === "counties")
                    && root.worldMap) {
                // These boundary sources are U.S.-only.  Center them when the
                // layer is enabled so a valid 56/3k item count is immediately
                // visible instead of looking like an empty world layer.
                root.worldMap.focusLocation(-98.5, 39.0, 72, 44)
            }
            if (layerId === "pota" || layerId === "states"
                    || layerId === "counties" || layerId === "iota"
                    || layerId === "wpx" || layerId === "earthquakes")
                root.syncOperations()
        }
    }
}
