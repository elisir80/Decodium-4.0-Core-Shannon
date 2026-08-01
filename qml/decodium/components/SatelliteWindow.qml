import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

pragma ComponentBehavior: Bound

Dialog {
    id: satelliteWindow
    title: qsTr("Satellite tracking")
    modal: false
    width: Math.max(560, Math.min(720, (parent ? parent.width : 720) - 48))
    height: Math.max(520, Math.min(760, (parent ? parent.height : 760) - 48))
    padding: 14
    closePolicy: Popup.CloseOnEscape

    property var bridge: (typeof appEngine !== "undefined") ? appEngine : null
    property var tracker: bridge ? bridge.satelliteTracking : null
    property var rotator: tracker ? tracker.rotator : null
    property color bgDeep: bridge ? bridge.themeManager.bgDeep : "#0b1220"
    property color bgMedium: bridge ? bridge.themeManager.bgMedium : "#121c2d"
    property color textPrimary: bridge ? bridge.themeManager.textPrimary : "#e5eefc"
    property color textSecondary: bridge ? bridge.themeManager.textSecondary : "#9db1c9"
    property color accent: bridge ? bridge.themeManager.secondaryColor : "#00d8ff"
    property color green: bridge ? bridge.themeManager.accentColor : "#2ecc71"
    property color amber: bridge ? bridge.themeManager.warningColor : "#f6c344"

    function updateObserver() {
        if (!tracker || !bridge) return
        tracker.setObserverGrid(String(bridge.grid || ""))
        tracker.nominalFrequencyHz = Number(bridge.frequency || 0)
    }

    function selectFirstSatellite() {
        if (!tracker || tracker.selectedSatellite || !tracker.satelliteNames
                || tracker.satelliteNames.length === 0)
            return
        tracker.selectSatellite(tracker.satelliteNames[0])
    }

    onAboutToShow: {
        updateObserver()
        selectFirstSatellite()
        if (tracker && tracker.upcomingPasses.length === 0)
            tracker.predictPassesAsync(24, 0)
    }

    background: Rectangle {
        color: Qt.rgba(bgDeep.r, bgDeep.g, bgDeep.b, 0.98)
        border.color: accent
        border.width: 2
        radius: 12
    }

    header: RowLayout {
        spacing: 10
        Text {
            text: qsTr("🛰 Satellite tracking")
            color: accent
            font.pixelSize: 18
            font.bold: true
            Layout.fillWidth: true
        }
        ToolButton {
            text: "×"
            onClicked: satelliteWindow.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: satelliteCombo
                Layout.fillWidth: true
                model: tracker ? tracker.satelliteNames : []
                currentIndex: {
                    if (!tracker || !tracker.selectedSatellite) return -1
                    return model.indexOf(tracker.selectedSatellite)
                }
                onActivated: {
                    if (tracker) tracker.selectSatellite(currentText)
                }
            }
            Button {
                text: tracker && tracker.updating ? qsTr("Updating…") : qsTr("Refresh TLE")
                enabled: !!tracker && !tracker.updating
                onClicked: tracker.refreshTle()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Button {
                text: tracker && tracker.tracking ? qsTr("Stop") : qsTr("Track")
                enabled: !!tracker && !!tracker.selectedSatellite
                onClicked: tracker.tracking ? tracker.stopTracking() : tracker.startTracking()
            }
            CheckBox {
                text: qsTr("Auto rotator")
                enabled: !!tracker
                checked: tracker ? tracker.autoRotator : false
                onToggled: if (tracker) tracker.autoRotator = checked
            }
            CheckBox {
                text: qsTr("Rotator enabled")
                enabled: !!tracker
                checked: tracker ? tracker.rotatorEnabled : false
                onToggled: if (tracker) tracker.rotatorEnabled = checked
            }
            CheckBox {
                text: qsTr("Auto Doppler")
                enabled: !!tracker
                checked: tracker ? tracker.dopplerTracking : false
                onToggled: if (tracker) tracker.dopplerTracking = checked
            }
            Item { Layout.fillWidth: true }
            Text {
                text: tracker ? tracker.statusMessage : qsTr("Satellite service unavailable")
                color: textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
                Layout.maximumWidth: 260
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: qsTr("Protocol"); color: textSecondary; font.pixelSize: 10 }
            ComboBox {
                Layout.preferredWidth: 130
                enabled: !!rotator
                model: rotator ? rotator.protocols : []
                currentIndex: rotator ? Math.max(0, model.indexOf(rotator.protocol)) : 0
                onActivated: if (rotator) rotator.protocol = currentText
            }
            TextField {
                Layout.fillWidth: true
                enabled: !!rotator
                text: rotator ? rotator.host : "127.0.0.1"
                placeholderText: qsTr("Rotator host")
                onEditingFinished: if (rotator) rotator.host = text
            }
            SpinBox {
                Layout.preferredWidth: 90
                enabled: !!rotator
                from: 1
                to: 65535
                editable: true
                value: rotator ? rotator.port : 12040
                onValueModified: if (rotator) rotator.port = value
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            color: Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.72)
            radius: 8
            border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.35)

            GridLayout {
                anchors.fill: parent
                anchors.margins: 12
                columns: 4
                rowSpacing: 8
                columnSpacing: 16
                Text { text: qsTr("Azimuth"); color: textSecondary }
                Text { text: tracker ? Number(tracker.azimuth).toFixed(1) + "°" : "—"; color: textPrimary; font.bold: true }
                Text { text: qsTr("Elevation"); color: textSecondary }
                Text { text: tracker ? Number(tracker.elevation).toFixed(1) + "°" : "—"; color: tracker && tracker.visible ? green : amber; font.bold: true }
                Text { text: qsTr("Range"); color: textSecondary }
                Text { text: tracker ? Number(tracker.rangeKm).toFixed(0) + " km" : "—"; color: textPrimary }
                Text { text: qsTr("Visibility"); color: textSecondary }
                Text { text: tracker ? (tracker.visible ? qsTr("VISIBLE") : qsTr("BELOW HORIZON")) : "—"; color: tracker && tracker.visible ? green : amber }
                Text { text: qsTr("Doppler"); color: textSecondary }
                Text { text: tracker ? Number(tracker.dopplerHz).toFixed(0) + " Hz" : "—"; color: textPrimary }
                Text { text: qsTr("Tracked frequency"); color: textSecondary }
                Text { text: tracker && tracker.dopplerFrequencyHz > 0 ? (tracker.dopplerFrequencyHz / 1000000).toFixed(6) + " MHz" : "—"; color: textPrimary }
                Text { text: qsTr("Observer"); color: textSecondary }
                Text { text: tracker ? (tracker.observerGrid || (Number(tracker.observerLatitude).toFixed(2) + ", " + Number(tracker.observerLongitude).toFixed(2))) : "—"; color: textPrimary }
                Text { text: qsTr("TLE age"); color: textSecondary }
                Text { text: tracker && tracker.tleUpdatedMs > 0 ? Qt.formatDateTime(new Date(tracker.tleUpdatedMs), "yyyy-MM-dd HH:mm") : "—"; color: textPrimary }
                Text { text: qsTr("Rotor feedback"); color: textSecondary }
                Text {
                    text: rotator && rotator.feedbackAvailable
                        ? qsTr("AZ %1° / EL %2°")
                              .arg(Number(rotator.currentAzimuth).toFixed(1))
                              .arg(Number(rotator.currentElevation).toFixed(1))
                        : qsTr("Unavailable")
                    color: rotator && rotator.feedbackAvailable ? green : amber
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("Rotor STOP")
                enabled: !!rotator
                onClicked: if (rotator) rotator.stop()
            }
            Button {
                text: qsTr("Rotor PARK")
                enabled: !!rotator
                onClicked: if (rotator) rotator.park()
            }
            Text {
                Layout.fillWidth: true
                text: rotator ? rotator.status : ""
                color: textSecondary
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: tracker && tracker.predictingPasses ? qsTr("Calculating passes…") : qsTr("Upcoming passes")
                color: accent
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                text: qsTr("Predict 24 h")
                enabled: !!tracker && !tracker.predictingPasses && !!tracker.selectedSatellite
                onClicked: tracker.predictPassesAsync(24, 0)
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: tracker ? tracker.upcomingPasses : []
            spacing: 4
            delegate: Rectangle {
                width: ListView.view.width
                height: 48
                radius: 5
                color: index % 2 ? Qt.rgba(bgMedium.r, bgMedium.g, bgMedium.b, 0.45) : "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 7
                    Text { text: modelData.aos || "—"; color: textPrimary; Layout.preferredWidth: 150 }
                    Text { text: qsTr("max %1°").arg(Number(modelData.maxElevation || 0).toFixed(1)); color: green; Layout.preferredWidth: 80 }
                    Text { text: qsTr("LOS %1").arg(modelData.los || "—"); color: textSecondary; Layout.fillWidth: true }
                    Text { text: qsTr("AZ %1°→%2°").arg(Number(modelData.aosAzimuth || 0).toFixed(0)).arg(Number(modelData.losAzimuth || 0).toFixed(0)); color: textSecondary; font.pixelSize: 10 }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: tracker && tracker.predictingPasses ? qsTr("Calculation running in background…") : qsTr("No pass in the selected interval")
                color: textSecondary
            }
        }
    }

    Connections {
        target: tracker
        ignoreUnknownSignals: true
        function onSatellitesChanged() { selectFirstSatellite() }
        function onSelectedSatelliteChanged() {
            if (tracker && tracker.selectedSatellite)
                tracker.predictPassesAsync(24, 0)
        }
    }
    Connections {
        target: bridge
        ignoreUnknownSignals: true
        function onGridChanged() { updateObserver() }
        function onFrequencyChanged() {
            if (tracker && !tracker.dopplerTracking)
                tracker.nominalFrequencyHz = Number(bridge.frequency || 0)
        }
    }
}
