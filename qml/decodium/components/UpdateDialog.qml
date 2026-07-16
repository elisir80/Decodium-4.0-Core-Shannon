import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// IU8LMC — Avviso di aggiornamento con conferma esplicita.
//
// Niente parte da solo: l'app avvisa, l'utente decide. "Aggiorna ora" scarica e
// lancia l'installer (che disinstalla da solo la versione precedente e propone
// il riavvio); "Piu' tardi" richiude e basta; "Salta questa versione" silenzia
// SOLO questa versione, non le successive.
Dialog {
    id: updateDialog
    title: qsTr("Update available")
    modal: true
    width: 640
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton

    Material.theme: Material.Dark
    Material.accent: Material.Orange

    property color bgDeep: "#1a1a2e"
    property color accentOrange: "#ff7814"

    background: Rectangle {
        color: bgDeep
        radius: 12
        border.color: accentOrange
        border.width: 1
        layer.enabled: false   // MultiEffect non c'e' su Qt 6.4 (AppImage Linux)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Decodium %1 is available").arg(updater.latestVersion)
            color: accentOrange
            font.pixelSize: 19
            font.bold: true
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("You are running %1.").arg(updater.currentVersion)
            color: "#c9d1d9"
            font.pixelSize: 13
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("What's new:")
            color: "white"
            font.pixelSize: 13
            font.bold: true
            visible: updater.releaseNotes.length > 0
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            visible: updater.releaseNotes.length > 0
            clip: true

            TextArea {
                text: updater.releaseNotes
                readOnly: true
                wrapMode: Text.Wrap
                color: "#c9d1d9"
                font.pixelSize: 12
                background: Rectangle { color: "#2a2a3e"; radius: 6 }
            }
        }

        // Avanzamento del download: senza, un utente con linea lenta pensa che
        // si sia piantato e chiude l'app a meta' scaricamento.
        ColumnLayout {
            Layout.fillWidth: true
            visible: updater.busy
            spacing: 4

            ProgressBar {
                Layout.fillWidth: true
                indeterminate: updater.progress < 0
                from: 0; to: 100
                value: updater.progress
            }
            Label {
                text: updater.statusText
                color: "#c9d1d9"
                font.pixelSize: 12
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !updater.busy
            text: qsTr("Decodium will close and the installer will start. Your settings and "
                       + "QSO log are kept.")
            color: "#8b949e"
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Skip this version")
                enabled: !updater.busy
                flat: true
                onClicked: { updater.skipThisVersion(); updateDialog.close() }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Later")
                enabled: !updater.busy
                flat: true
                onClicked: updateDialog.close()
            }

            Button {
                text: qsTr("Update now")
                enabled: !updater.busy
                highlighted: true
                onClicked: updater.downloadAndInstall()
            }
        }
    }
}
