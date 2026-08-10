/* Decodium Qt6 - DECOMETER RF Vector Meter
 * Strumento di misura RF: potenza diretta/riflessa, ROS, ALC.
 * Le grandezze provengono dalla telemetria CAT gia' letta dal rig
 * (RFPOWER_METER_WATTS / SWR / ALC); quanto non e' misurabile senza
 * un sensore vettoriale resta dichiarato come tale.
 * Disegno: Claude Design "DECOMETER RF Vector Meter".
 * By IU8LMC
 */

import QtQuick
import QtQuick.Controls

Dialog {
    id: decometerWindow
    title: qsTr("RF Meter")
    modal: false
    padding: 0
    closePolicy: Popup.CloseOnEscape

    readonly property int faceWidth: 900
    readonly property int faceHeight: 420

    // Il Dialog vive nell'overlay della finestra: dimensionarsi sul proprio
    // parent (il Loader) creerebbe un anello di binding e lo farebbe collassare.
    anchors.centerIn: Overlay.overlay
    readonly property real hostWidth: Overlay.overlay ? Overlay.overlay.width : faceWidth + 24
    readonly property real hostHeight: Overlay.overlay ? Overlay.overlay.height : faceHeight + 24

    width: Math.min(faceWidth + 24, Math.max(320, hostWidth - 32))
    height: Math.min(faceHeight + 24, Math.max(220, hostHeight - 32))

    // ---- tavolozza dello strumento (fissa: e' un frontalino, non un tema) ----
    readonly property color colInk:      "#E8ECEF"
    readonly property color colCyan:     "#27C4D4"
    readonly property color colGreen:    "#46D67C"
    readonly property color colAmber:    "#FFB454"
    readonly property color colRed:      "#FF4A4A"
    readonly property color colMuted:    "#5B6670"
    readonly property color colLabel:    "#8A939C"
    readonly property color colDim:      "#4E5A63"
    readonly property color colEdge:     "#262D34"
    readonly property color colPanel:    "#14181D"

    // ---------------------------------------------------------------- misure
    // Sorgenti reali. Nessun valore viene inventato: se il rig non fornisce
    // il dato, il display resta muto.
    readonly property bool  catUp:     !!(typeof bridge !== "undefined" && bridge.catConnected)
    readonly property bool  txOn:      !!(typeof bridge !== "undefined" && bridge.transmitting)
    readonly property real  rawFwd:    (typeof bridge !== "undefined" && bridge.rigPowerWatts > 0) ? bridge.rigPowerWatts : 0
    readonly property real  rawSwr:    (typeof bridge !== "undefined" && bridge.rigSwr >= 1) ? bridge.rigSwr : 1
    readonly property bool  swrValid:  catUp && (typeof bridge !== "undefined") && bridge.rigSwr >= 1
    readonly property bool  pwrValid:  catUp && rawFwd > 0
    readonly property real  rawAlc:    (typeof bridge !== "undefined") ? bridge.rigAlc : 0
    readonly property bool  alcValid:  catUp && (typeof bridge !== "undefined") && bridge.rigAlcValid

    // coefficiente di riflessione: rho = (ROS-1)/(ROS+1)
    readonly property real  rho:       rawSwr > 1 ? (rawSwr - 1) / (rawSwr + 1) : 0

    // stato ballistico (aggiornato dal tick, non nei binding)
    property real vFwd: 0
    property real vRef: 0
    property real vSwr: 1
    property real pkFwdV: 0;  property real pkFwdT: 0
    property real pkRefV: 0;  property real pkRefT: 0
    property real pkSwrV: 0;  property real pkSwrT: 0
    property real pepW: 0
    property real avgW: 0
    property real txSeconds: 0

    // portate: 5 / 50 / 500 / 5000 W
    property int  rangeIdx: 1
    property bool autoRange: true
    property real rangeFrom: 50
    property real rangeTo: 50
    property real rangeT0: -9
    property real overT: 0
    readonly property var fsArr: [5, 50, 500, 5000]
    readonly property var fsMult: ["×0.1", "×1", "×10", "×100"]
    readonly property var fsWatt: ["5 W", "50 W", "500 W", "5 kW"]

    property int  screenIdx: 0
    readonly property int screenCount: 3

    property real clock: 0

    function nowS() { return clock }

    function setRange(i) {
        rangeFrom = fsArr[rangeIdx]
        rangeTo = fsArr[i]
        rangeT0 = clock
        rangeIdx = i
        autoRange = false
    }

    // portata effettiva con transizione morbida (0.35 s), come da disegno
    function effFs() {
        var d = clock - rangeT0
        if (d >= 0 && d < 0.35) {
            var e = d / 0.35
            var k = e * e * (3 - 2 * e)
            return rangeFrom + (rangeTo - rangeFrom) * k
        }
        return fsArr[rangeIdx]
    }

    function fmtW(v) { return v >= 100 ? v.toFixed(1) : v.toFixed(2) }

    // ---- grandezze derivate (fisica esatta, non stime) ----
    readonly property real returnLossDb: rho > 0.0005 ? -20 * Math.log(rho) / Math.LN10 : 99
    readonly property real mismatchLossDb: rho > 0.0005 ? -10 * Math.log(1 - rho * rho) / Math.LN10 : 0
    readonly property real netW: Math.max(0, vFwd - vRef)
    // con solo il modulo di Gamma la resistenza e' vincolata a un intervallo:
    // 50/ROS <= R <= 50*ROS. La reattanza richiede la fase, che il CAT non da'.
    readonly property real rMin: 50 / Math.max(1, rawSwr)
    readonly property real rMax: 50 * Math.max(1, rawSwr)

    onVisibleChanged: {
        if (visible) {
            clock = 0
            txSeconds = 0
            pepW = 0
            face.forceActiveFocus()
        }
    }

    background: Rectangle {
        color: "#0B0E11"
        border.color: "#23292F"
        border.width: 1
        radius: 10
    }

    // riga di misura riusata dalle tre schermate del display
    component Readout: Item {
        property string tag: ""
        property string value: ""
        property string unit: ""
        property color tint: "#E8ECEF"
        property int valueSize: 17
        width: parent ? parent.width : 0
        height: valueSize + 4
        Text {
            id: tagText
            anchors.left: parent.left
            anchors.baseline: valText.baseline
            text: parent.tag
            width: 46
            font.pixelSize: 13; font.bold: true; font.family: "monospace"
            color: parent.tint
        }
        Text {
            id: unitText
            anchors.right: parent.right
            anchors.baseline: valText.baseline
            text: parent.unit
            font.pixelSize: 10; font.family: "monospace"
            color: parent.tint
        }
        Text {
            id: valText
            anchors.left: tagText.right
            anchors.right: unitText.left
            anchors.rightMargin: 8
            horizontalAlignment: Text.AlignRight
            text: parent.value
            font.pixelSize: parent.valueSize; font.bold: true; font.family: "monospace"
            color: parent.tint
        }
    }

    // ------------------------------------------------------------ ballistica
    // 25 Hz in trasmissione, 5 Hz a riposo: uno strumento non deve pesare
    // sui PC modesti quando non c'e' nulla da mostrare.
    // 25 Hz finche' c'e' qualcosa da mostrare (trasmissione in corso oppure
    // aghi e picchi ancora in discesa), 5 Hz a strumento fermo: la discesa
    // non deve andare a scatti, ma nemmeno pesare sui PC modesti.
    readonly property bool settling: vFwd > 0.01 || vRef > 0.001
                                     || pkFwdV > 0.01 || pkRefV > 0.001 || pkSwrV > 0.002

    Timer {
        id: engine
        interval: (decometerWindow.txOn || decometerWindow.settling) ? 40 : 200
        running: decometerWindow.visible
        repeat: true
        onTriggered: decometerWindow.tick(interval / 1000)
    }

    function tick(dt) {
        clock += dt

        var fwdT = (txOn && pwrValid) ? rawFwd : 0
        var swrT = (txOn && swrValid) ? rawSwr : vSwr

        // attacco istantaneo, rilascio esponenziale (0.5 s potenza, 0.4 s ROS)
        function rel(cur, tgt, tau) {
            return tgt >= cur ? tgt : cur + (tgt - cur) * (1 - Math.exp(-dt / tau))
        }
        var refT = fwdT * rho * rho

        vFwd = rel(vFwd, fwdT, 0.5)
        vRef = rel(vRef, refT, 0.5)
        if (txOn && swrValid)
            vSwr = rel(vSwr, swrT, 0.4)

        // ritenuta di picco 3 s, poi discesa con tau 0.9 s
        function peak(v, pv, pt) {
            if (v >= pv - 1e-9) return [v, clock]
            if (clock - pt > 3) return [pv + (0 - pv) * (1 - Math.exp(-dt / 0.9)), pt]
            return [pv, pt]
        }
        var p
        p = peak(vFwd, pkFwdV, pkFwdT); pkFwdV = p[0]; pkFwdT = p[1]
        p = peak(vRef, pkRefV, pkRefT); pkRefV = p[0]; pkRefT = p[1]
        var sF = Math.max(0, (vSwr - 1) / (vSwr + 1))
        p = peak(sF, pkSwrV, pkSwrT); pkSwrV = p[0]; pkSwrT = p[1]

        if (txOn) {
            txSeconds += dt
            if (vFwd > pepW) pepW = vFwd
            avgW = avgW + (vFwd - avgW) * (1 - Math.exp(-dt / 3.0))
        }

        // cambio portata automatico: oltre il 95% del fondo scala per 0.5 s
        if (autoRange && vFwd > 0.95 * fsArr[rangeIdx] && rangeIdx < 3) {
            overT += dt
            if (overT > 0.5) { setRangeAuto(rangeIdx + 1); overT = 0 }
        } else if (autoRange && rangeIdx > 0 && pkFwdV < 0.35 * fsArr[rangeIdx - 1]) {
            overT -= dt
            if (overT < -2.5) { setRangeAuto(rangeIdx - 1); overT = 0 }
        } else {
            overT = 0
        }

        gauge.requestPaint()
    }

    function setRangeAuto(i) {
        rangeFrom = fsArr[rangeIdx]
        rangeTo = fsArr[i]
        rangeT0 = clock
        rangeIdx = i
    }

    contentItem: Item {
        id: faceHolder
        clip: true

        // il frontalino ha proporzioni fisse: si adatta scalando, non deformando
        readonly property real fit: Math.min(width / decometerWindow.faceWidth,
                                            height / decometerWindow.faceHeight, 1)

        Item {
            id: face
            width: decometerWindow.faceWidth
            height: decometerWindow.faceHeight
            anchors.centerIn: parent
            scale: faceHolder.fit
            focus: true

            Keys.onLeftPressed:  decometerWindow.screenIdx = (decometerWindow.screenIdx + decometerWindow.screenCount - 1) % decometerWindow.screenCount
            Keys.onRightPressed: decometerWindow.screenIdx = (decometerWindow.screenIdx + 1) % decometerWindow.screenCount

            Rectangle {
                anchors.fill: parent
                radius: 10
                border.color: "#23292F"
                border.width: 1
                gradient: Gradient {
                    GradientStop { position: 0.0;  color: "#171B21" }
                    GradientStop { position: 0.55; color: "#12161B" }
                    GradientStop { position: 1.0;  color: "#101418" }
                }
            }

            // ------------------------------------------------- scale ad arco
            Canvas {
                id: gauge
                x: 14; y: 6
                width: 640; height: 264
                renderStrategy: Canvas.Immediate

                onPaint: {
                    var g = getContext("2d")
                    g.clearRect(0, 0, 640, 264)

                    var cx = 320, cy = 640
                    var A0 = -Math.PI * 2 / 3, A1 = -Math.PI / 3
                    var dim = decometerWindow.txOn ? 1 : 0.3
                    var GREEN = decometerWindow.colGreen
                    var AMBER = decometerWindow.colAmber
                    var RED = decometerWindow.colRed

                    function ang(f) { return A0 + (A1 - A0) * f }
                    function tick(R, f, len) {
                        var a = ang(f)
                        g.beginPath()
                        g.moveTo(cx + R * Math.cos(a), cy + R * Math.sin(a))
                        g.lineTo(cx + (R + len) * Math.cos(a), cy + (R + len) * Math.sin(a))
                        g.stroke()
                    }
                    function lbl(R, f, s) {
                        var a = ang(f)
                        g.fillText(s, cx + R * Math.cos(a), cy + R * Math.sin(a) + 3)
                    }

                    g.textAlign = "center"
                    g.font = "9px monospace"
                    g.strokeStyle = "#3A424A"
                    g.lineWidth = 1
                    g.fillStyle = "#6A737C"

                    // fondo scala della portata attiva, in watt
                    var fs = decometerWindow.effFs()
                    var stepW = fs / 10
                    for (var i = 0; i <= 10; i++) {
                        tick(601, i / 10, 5)
                        var v = stepW * i
                        lbl(616, i / 10, v >= 1000 ? (v / 1000) + "k" : String(Math.round(v * 100) / 100))
                    }
                    // scala riflessa: fondo scala = 20% della diretta
                    for (var j = 0; j <= 5; j++) {
                        tick(536, j / 5, 5)
                        var vr = fs * 0.2 / 5 * j
                        lbl(550, j / 5, vr >= 1000 ? (vr / 1000) + "k" : String(Math.round(vr * 100) / 100))
                    }
                    var swrL = [[1, "1.0"], [1.25, "1.25"], [1.5, "1.5"], [2, "2"], [3, "3"], [5, "5"], [1e9, "∞"]]
                    for (var k = 0; k < swrL.length; k++) {
                        var s = swrL[k][0]
                        var f = Math.min(1, (s - 1) / (s + 1))
                        tick(476, f, 5)
                        lbl(490, f, swrL[k][1])
                    }
                    g.font = "8px monospace"
                    g.fillStyle = "#525C64"
                    var alcT = [[0.25, "25"], [0.5, "50"], [0.75, "75"]]
                    for (var q = 0; q < alcT.length; q++) {
                        tick(452, alcT[q][0], -4)
                        lbl(443, alcT[q][0], alcT[q][1])
                    }

                    g.font = "bold 9px sans-serif"
                    g.fillStyle = "#7A848D"
                    g.textAlign = "left"
                    g.fillText("FWD", 18, 88)
                    g.fillText("REF", 52, 150)
                    g.fillText("SWR", 87, 212)
                    g.font = "bold 8px sans-serif"
                    g.fillStyle = "#525C64"
                    g.fillText("ALC %", 126, 247)

                    function arc(R, len, n, litF, peakF, colFn) {
                        var step = (A1 - A0) / n
                        for (var i2 = 0; i2 < n; i2++) {
                            var ff = (i2 + 0.5) / n
                            var a2 = A0 + step * (i2 + 0.5)
                            var ca = Math.cos(a2), sa = Math.sin(a2)
                            g.beginPath()
                            g.moveTo(cx + R * ca, cy + R * sa)
                            g.lineTo(cx + (R + len) * ca, cy + (R + len) * sa)
                            if (ff <= litF) {
                                g.strokeStyle = colFn(ff)
                                g.lineWidth = 5
                                g.globalAlpha = dim
                                g.stroke()
                                if (dim === 1) {
                                    g.globalAlpha = 0.22
                                    g.lineWidth = 9
                                    g.stroke()
                                }
                            } else {
                                g.strokeStyle = decometerWindow.colInk
                                g.globalAlpha = 0.08
                                g.lineWidth = 5
                                g.stroke()
                            }
                            g.globalAlpha = 1
                        }
                        if (peakF > 0.012) {
                            var ip = Math.min(n - 1, Math.floor(peakF * n))
                            var ap = A0 + step * (ip + 0.5)
                            var cp = Math.cos(ap), sp = Math.sin(ap)
                            g.beginPath()
                            g.moveTo(cx + R * cp, cy + R * sp)
                            g.lineTo(cx + (R + len) * cp, cy + (R + len) * sp)
                            g.strokeStyle = colFn(Math.min(1, peakF))
                            g.lineWidth = 5
                            g.globalAlpha = Math.max(dim, 0.95)
                            g.stroke()
                            g.globalAlpha = 0.4
                            g.lineWidth = 11
                            g.stroke()
                            g.globalAlpha = 1
                        }
                    }

                    function pwCol(f) { return f < 0.7 ? GREEN : (f < 0.9 ? AMBER : RED) }
                    function swCol(f) { return f < 0.2 ? GREEN : (f < 0.5 ? AMBER : RED) }
                    function cl(v) { return Math.max(0, Math.min(1, v)) }

                    var sFnow = Math.max(0, (decometerWindow.vSwr - 1) / (decometerWindow.vSwr + 1))
                    arc(580, 20, 64, cl(decometerWindow.vFwd / fs), cl(decometerWindow.pkFwdV / fs), pwCol)
                    arc(520, 15, 46, cl(decometerWindow.vRef / (fs * 0.2)), cl(decometerWindow.pkRefV / (fs * 0.2)), pwCol)
                    arc(460, 15, 34,
                        decometerWindow.swrValid ? cl(sFnow) : 0,
                        decometerWindow.swrValid ? cl(decometerWindow.pkSwrV) : 0, swCol)

                    // arco ALC piu' interno: percentuale di compressione del rig
                    if (decometerWindow.alcValid) {
                        arc(430, 10, 24, cl(decometerWindow.rawAlc / 100), 0,
                            function (f) { return f < 0.6 ? GREEN : (f < 0.85 ? AMBER : RED) })
                    }
                }
            }

            Rectangle {
                x: 660; y: 20; width: 1; height: 380
                gradient: Gradient {
                    GradientStop { position: 0.0;  color: "transparent" }
                    GradientStop { position: 0.2;  color: "#262D34" }
                    GradientStop { position: 0.8;  color: "#262D34" }
                    GradientStop { position: 1.0;  color: "transparent" }
                }
            }

            // ------------------------------------------------------- portate
            Row {
                x: 170; y: 250; width: 430; height: 30
                spacing: 8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("RANGE")
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    color: decometerWindow.colMuted
                    rightPadding: 4
                }

                Repeater {
                    model: 4
                    delegate: Rectangle {
                        id: rangeChip
                        required property int index
                        readonly property bool on: index === decometerWindow.rangeIdx
                        width: 62; height: 30; radius: 5
                        color: on ? Qt.rgba(0.153, 0.769, 0.831, 0.14) : decometerWindow.colPanel
                        border.width: 1
                        border.color: on ? decometerWindow.colCyan
                                         : (rangeMouse.containsMouse ? "#3A424A" : decometerWindow.colEdge)
                        Column {
                            anchors.centerIn: parent
                            spacing: 1
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: decometerWindow.fsMult[rangeChip.index]
                                font.pixelSize: 10; font.bold: true; font.family: "monospace"
                                color: rangeChip.on ? decometerWindow.colCyan : "#7A848D"
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: decometerWindow.fsWatt[rangeChip.index]
                                font.pixelSize: 8; font.family: "monospace"
                                opacity: 0.75
                                color: rangeChip.on ? decometerWindow.colCyan : "#7A848D"
                            }
                        }
                        MouseArea {
                            id: rangeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: decometerWindow.setRange(rangeChip.index)
                        }
                    }
                }
            }

            // ------------------------------------------------ display numerico
            Rectangle {
                x: 170; y: 296; width: 430; height: 106
                radius: 6
                color: "#000000"
                border.width: 1
                border.color: decometerWindow.swrValid && decometerWindow.vSwr >= 3 ? decometerWindow.colRed : "#1E252C"

                Column {
                    anchors.fill: parent
                    anchors.topMargin: 7
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.bottomMargin: 8
                    spacing: 5
                    opacity: decometerWindow.txOn ? 1 : 0.45

                    Item {
                        width: parent.width; height: 11
                        Text {
                            anchors.left: parent.left
                            text: decometerWindow.catUp
                                  ? ("CAT: " + (bridge.catRigName.length ? bridge.catRigName : qsTr("connected")).toUpperCase())
                                  : qsTr("NO CAT TELEMETRY")
                            font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                            color: decometerWindow.colDim
                        }
                        Text {
                            anchors.right: parent.right
                            text: decometerWindow.txOn ? (decometerWindow.screenIdx === 2 ? "TX-AVG" : "TX-PK") : "RX"
                            font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                            color: decometerWindow.txOn ? decometerWindow.colCyan : decometerWindow.colDim
                        }
                    }

                    Row {
                        width: parent.width
                        height: 72
                        spacing: 18

                        Column {
                            width: parent.width - 148
                            spacing: 2

                            // schermata 1 - potenza
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "FWD"; tint: decometerWindow.colCyan
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.pkFwdV) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "REF"
                                value: decometerWindow.pwrValid && decometerWindow.swrValid
                                       ? decometerWindow.pkRefV.toFixed(3) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 0
                                tag: "SWR"; tint: decometerWindow.colAmber; valueSize: 22
                                value: decometerWindow.swrValid ? decometerWindow.vSwr.toFixed(2) : "——"
                            }

                            // schermata 2 - adattamento
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "RL"; tint: decometerWindow.colCyan
                                value: decometerWindow.swrValid
                                       ? (decometerWindow.returnLossDb >= 99 ? "> 60" : decometerWindow.returnLossDb.toFixed(1))
                                       : "——"
                                unit: "dB"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "ML"
                                value: decometerWindow.swrValid ? decometerWindow.mismatchLossDb.toFixed(2) : "——"
                                unit: "dB"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 1
                                tag: "NET"; tint: decometerWindow.colGreen; valueSize: 22
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.netW) : "——"
                                unit: "W"
                            }

                            // schermata 3 - pilotaggio
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "ALC"; tint: decometerWindow.colAmber
                                value: decometerWindow.alcValid ? Math.round(decometerWindow.rawAlc) + "" : "——"
                                unit: "%"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "PEP"; tint: decometerWindow.colCyan
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.pepW) : "——"
                                unit: "W"
                            }
                            Readout {
                                visible: decometerWindow.screenIdx === 2
                                tag: "AVG"; valueSize: 22
                                value: decometerWindow.pwrValid ? decometerWindow.fmtW(decometerWindow.avgW) : "——"
                                unit: "W"
                            }
                        }

                        // colonna impedenza: dichiara cosa e' noto e cosa no
                        Item {
                            width: 130; height: parent.height
                            Rectangle { width: 1; height: parent.height; color: "#1A2228" }
                            Column {
                                x: 14
                                spacing: 3
                                Text {
                                    text: decometerWindow.screenIdx === 2 ? qsTr("TX TIME") : qsTr("IMPEDANCE")
                                    font.pixelSize: 9; font.letterSpacing: 1; font.family: "monospace"
                                    color: decometerWindow.colDim
                                    bottomPadding: 3
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2
                                    text: decometerWindow.swrValid
                                          ? "|Γ| " + decometerWindow.rho.toFixed(3) : "|Γ| —"
                                    font.pixelSize: 13; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2
                                    text: decometerWindow.swrValid
                                          ? "R " + decometerWindow.rMin.toFixed(1) + "–" + decometerWindow.rMax.toFixed(1)
                                          : "R —"
                                    font.pixelSize: 13; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                                Text {
                                    visible: decometerWindow.screenIdx !== 2
                                    text: qsTr("X: needs vector sensor")
                                    width: 116
                                    wrapMode: Text.WordWrap
                                    font.pixelSize: 8
                                    color: decometerWindow.colDim
                                }
                                Text {
                                    visible: decometerWindow.screenIdx === 2
                                    text: {
                                        var s = Math.floor(decometerWindow.txSeconds)
                                        return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + (s % 60)
                                    }
                                    font.pixelSize: 17; font.bold: true; font.family: "monospace"
                                    color: "#9FB3BC"
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------- spie
            Column {
                x: 684; y: 34
                spacing: 15

                Repeater {
                    model: [
                        { key: "swr",  label: qsTr("SWR ALARM") },
                        { key: "alc",  label: qsTr("ALC CLIP") },
                        { key: "pwr",  label: qsTr("PWR SENSE") },
                        { key: "cat",  label: qsTr("CAT LINK") }
                    ]
                    delegate: Row {
                        id: ledRow
                        required property var modelData
                        spacing: 11
                        readonly property bool lit: {
                            switch (ledRow.modelData.key) {
                            case "swr": return decometerWindow.swrValid && decometerWindow.vSwr >= 3
                                               && (Math.floor(decometerWindow.clock * 2.4) % 2 === 0)
                            case "alc": return decometerWindow.alcValid && decometerWindow.rawAlc >= 85
                            case "pwr": return decometerWindow.pwrValid
                            case "cat": return decometerWindow.catUp
                            }
                            return false
                        }
                        readonly property color hue: (ledRow.modelData.key === "swr" || ledRow.modelData.key === "alc")
                                                     ? decometerWindow.colRed : decometerWindow.colGreen
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: ledRow.lit ? ledRow.hue : "#20262C"
                            border.width: 1
                            border.color: Qt.rgba(0, 0, 0, 0.6)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: ledRow.modelData.label
                            font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.8
                            color: decometerWindow.colLabel
                        }
                    }
                }
            }

            // ------------------------------------------------ schermate e auto
            Column {
                x: 684; y: 196; width: 190
                spacing: 10

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "◀   " + qsTr("SCREEN") + "   ▶"
                    font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    color: decometerWindow.colMuted
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10
                    Repeater {
                        model: ["◀", "▶"]
                        delegate: Rectangle {
                            id: navChip
                            required property int index
                            required property string modelData
                            width: 74; height: 32; radius: 5
                            border.width: 1
                            border.color: nav.containsMouse ? "#3A424A" : "#2A3138"
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "#1C2127" }
                                GradientStop { position: 1.0; color: "#14181D" }
                            }
                            Text {
                                anchors.centerIn: parent
                                text: navChip.modelData
                                font.pixelSize: 11; font.bold: true
                                color: nav.containsMouse ? "#B9C2C9" : decometerWindow.colLabel
                            }
                            MouseArea {
                                id: nav
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    var n = decometerWindow.screenCount
                                    decometerWindow.screenIdx =
                                        (decometerWindow.screenIdx + (navChip.index === 0 ? n - 1 : 1)) % n
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 158; height: 34; radius: 5
                    color: decometerWindow.autoRange ? Qt.rgba(0.153, 0.769, 0.831, 0.14) : "#181D22"
                    border.width: 1
                    border.color: decometerWindow.autoRange ? decometerWindow.colCyan
                                                            : (autoMouse.containsMouse ? "#3A424A" : "#2A3138")
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("AUTO")
                        font.pixelSize: 10; font.bold: true; font.letterSpacing: 2
                        color: decometerWindow.autoRange ? decometerWindow.colCyan : decometerWindow.colLabel
                    }
                    MouseArea {
                        id: autoMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: decometerWindow.autoRange = !decometerWindow.autoRange
                    }
                }
            }

            // ------------------------------------------------------- marchio
            Column {
                x: 22; y: decometerWindow.faceHeight - 46
                spacing: 3
                Text {
                    textFormat: Text.StyledText
                    text: "DEC<font color=\"#27C4D4\">Ø</font>METER"
                    font.pixelSize: 12; font.bold: true; font.letterSpacing: 1.5
                    color: decometerWindow.colLabel
                }
                Text {
                    text: qsTr("RF VECTOR METER") + " · 1.8–500 MHz"
                    font.pixelSize: 8; font.letterSpacing: 1.2
                    color: decometerWindow.colMuted
                }
            }

            Text {
                x: decometerWindow.faceWidth - 92
                y: decometerWindow.faceHeight - 24
                text: "DECODIUM"
                font.pixelSize: 8; font.bold: true; font.letterSpacing: 2.5
                color: decometerWindow.colDim
            }
        }
    }
}
