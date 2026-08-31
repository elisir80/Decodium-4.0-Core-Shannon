// DecoRTTY — station details and macro editing.
import QtQuick
import QtQuick.Controls

import "."

Dialog {
    id: root

    modal: true
    visible: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: Overlay.overlay
    // Mai piu' grande della finestra che lo ospita. Con una misura fissa un
    // dialogo piu' alto della finestra usciva dai bordi, e quello che si vedeva
    // intorno non era piu' RTTY: era il velo dell'overlay steso sopra Decodium.
    // Si misura sull'overlay, che copre l'area della finestra ed e' lo stesso
    // su cui il dialogo si centra. Non su Window.window: un Dialog non deriva
    // da Item e quella proprieta' li' non vale.
    width: Math.min(620, Overlay.overlay ? Overlay.overlay.width - 24 : 620)
    height: Math.min(590, Overlay.overlay ? Overlay.overlay.height - 24 : 590)
    padding: 0

    background: GlassPanel { tintOpacity: 0.97 }

    // Il contenuto scorre. Con un'altezza fissa quello che non entrava
    // spariva e basta: su uno schermo con la scalatura di Windows al 150% i
    // punti disponibili sono molti meno di quelli che il conto sulla carta
    // suggerisce, e la parte in fondo di questa finestra non si vedeva —
    // senza nemmeno una barra a dire che c'era dell'altro.
    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    Column {
        width: parent.width
        spacing: 12

        Item { width: 1; height: 6 }

        // ── lingua ──────────────────────────────────────────────────────
        //
        // In cima e non in fondo: chi apre questa finestra perche' non capisce
        // quello che c'e' scritto deve trovarla subito, e i nomi delle lingue
        // sono scritti ciascuno nella propria — cercare "Deutsch" in un elenco
        // che dice "German" e' un ostacolo proprio per chi ha piu' bisogno.
        Row {
            x: 16
            spacing: 10

            PanelHeading {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("LANGUAGE")
            }

            ComboBox {
                id: comboLingua
                anchors.verticalCenter: parent.verticalCenter
                width: 190
                height: 30
                font.pixelSize: 12

                // Vestito come il pannello, non come il tema dei controlli.
                // Senza questo prende il fondo chiaro di Material e diventa una
                // barra grigia su un pannello scuro — quella che copriva la
                // scritta LINGUA qui accanto.
                background: Rectangle {
                    radius: 6
                    color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
                    border.color: comboLingua.activeFocus ? Theme.secondary : Theme.glassBorder
                    border.width: 1
                }
                contentItem: Text {
                    leftPadding: 10
                    rightPadding: comboLingua.indicator.width + 6
                    text: comboLingua.displayText
                    color: Theme.textPrimary
                    font: comboLingua.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                model: language.available
                textRole: "name"
                valueRole: "code"
                currentIndex: {
                    for (let i = 0; i < model.length; ++i)
                        if (model[i].code === language.current)
                            return i
                    return 0
                }
                onActivated: language.current = currentValue
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("changes immediately")
                color: Theme.textSecondary
                font.pixelSize: 10
            }
        }

        Rectangle {
            x: 16
            width: parent.width - 32
            height: 1
            color: Theme.glassBorder
        }

        PanelHeading {
            x: 16
            text: qsTr("STATION")
        }

        Row {
            x: 16
            spacing: 10

            QsoField {
                label: qsTr("MY CALL")
                width: 130
                text: macros.myCall
                highlight: true
                onEdited: (value) => macros.myCall = value
            }
            QsoField {
                label: qsTr("NAME")
                width: 130
                text: macros.myName
                onEdited: (value) => macros.myName = value
            }
            QsoField {
                label: qsTr("QTH")
                width: 160
                text: macros.myQth
                onEdited: (value) => macros.myQth = value
            }
        }

        Rectangle {
            x: 16
            width: parent.width - 32
            height: 1
            color: Theme.glassBorder
        }

        Rectangle {
            x: 16
            width: parent.width - 32
            height: 1
            color: Theme.glassBorder
        }

        Row {
            x: 16
            width: parent.width - 32

            PanelHeading {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("MACROS")
            }
        }

        // I segnaposto su una riga propria, che va a capo. Messi in coda
        // all'intestazione uscivano dal bordo destro e si leggevano a meta':
        // sono l'unica cosa in questa finestra che l'operatore deve poter
        // copiare con l'occhio mentre scrive una macro.
        Text {
            x: 16
            width: parent.width - 32
            wrapMode: Text.WordWrap
            text: "%MYCALL% %HISCALL% %RST% %NAME% %QTH% %SERIAL% %TIME% %DATE%"
            color: Theme.textSecondary
            font.pixelSize: 9
            font.family: Theme.monoFamily
        }

        ListView {
            x: 12
            width: parent.width - 24
            // Alta quanto il suo contenuto, e non scorrevole: a scorrere ci
            // pensa il dialogo. Con due scorrimenti annidati la lista si
            // muoveva dentro una finestra che si muoveva a sua volta, e la
            // prima macro finiva tagliata a meta' sotto l'intestazione.
            height: contentHeight
            interactive: false
            spacing: 5
            model: macros

            delegate: Row {
                required property int index
                required property string label
                required property string macroTemplate

                width: ListView.view.width
                spacing: 6

                Rectangle {
                    width: 96
                    height: 30
                    radius: 6
                    color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
                    border.color: Theme.glassBorder

                    TextInput {
                        id: labelInput
        // Ritagliato al proprio riquadro: senza questo un testo piu' lungo
        // del campo continua a disegnarsi oltre il bordo, sopra quello che
        // gli sta accanto.
        clip: true
                        anchors.fill: parent
                        anchors.margins: 7
                        verticalAlignment: TextInput.AlignVCenter
                        text: label
                        color: Theme.textPrimary
                        font.pixelSize: 12
                        selectByMouse: true
                        onEditingFinished: macros.setMacro(index, text, macroTemplate)
                    }
                }

                Rectangle {
                    width: parent.width - 108
                    height: 30
                    radius: 6
                    color: Qt.rgba(Theme.bgDeep.r, Theme.bgDeep.g, Theme.bgDeep.b, 0.85)
                    border.color: templateInput.activeFocus ? Theme.secondary : Theme.glassBorder

                    TextInput {
                        id: templateInput
        // Ritagliato al proprio riquadro: senza questo un testo piu' lungo
        // del campo continua a disegnarsi oltre il bordo, sopra quello che
        // gli sta accanto.
        clip: true
                        anchors.fill: parent
                        anchors.margins: 7
                        verticalAlignment: TextInput.AlignVCenter
                        // CR and LF are real characters in a macro; showing them
                        // as escapes keeps the field editable in one line.
                        text: macroTemplate.replace(/\r/g, "\\r").replace(/\n/g, "\\n")
                        color: Theme.textPrimary
                        font.family: Theme.monoFamily
                        font.pixelSize: 12
                        selectByMouse: true
                        onEditingFinished: {
                            const expanded = text.replace(/\\r/g, "\r").replace(/\\n/g, "\n")
                            macros.setMacro(index, labelInput.text, expanded)
                        }
                    }
                }
            }
        }

        Row {
            x: 16
            spacing: 8

            GlassButton {
                text: qsTr("Restore defaults")
                minimumWidth: 140
                accentColor: Theme.warning
                onClicked: macros.resetToDefaults()
            }
            GlassButton {
                text: qsTr("Close")
                minimumWidth: 84
                onClicked: root.close()
            }
        }
    }
    }
}
