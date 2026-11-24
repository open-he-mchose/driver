// SPDX-License-Identifier: GPL-3.0-only
// Travel of one key over the last seconds (live depth stream), with its actuation point; the time the keyboard
// reports the key as pressed is shaded, which shows how rapid trigger behaves.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: graph
    property int tracked: -1          // matrix index; -1 = follow the last moved key
    property int shown: -1            // key actually drawn
    property var info: ({})
    property var samples: []          // {t, mm, pressed}
    readonly property real windowMs: 4000
    readonly property real maxMm: 3.4
    spacing: 4

    function refreshInfo() { info = shown >= 0 ? backend.keys.keyInfo(shown) : ({}) }
    onShownChanged: canvas.requestPaint()
    onInfoChanged: canvas.requestPaint()
    onTrackedChanged: { if (tracked >= 0) { shown = tracked; samples = []; refreshInfo() } }

    Connections {
        target: backend.keys
        function onDepthSample(matrixIndex, mm, pressed) {
            if (graph.tracked < 0 && matrixIndex !== graph.shown && mm > 0.2) {
                graph.shown = matrixIndex
                graph.samples = []
                graph.refreshInfo()
            }
            if (matrixIndex !== graph.shown)
                return
            const now = Date.now()
            graph.samples.push({ t: now, mm: mm, pressed: pressed })
            while (graph.samples.length > 2 && graph.samples[1].t < now - graph.windowMs)
                graph.samples.shift()
        }
    }
    Connections {
        target: backend
        function onSnapshotChanged() { graph.refreshInfo() }
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            font.bold: true
            text: graph.shown < 0 ? qsTr("Key depth") : qsTr("Key depth: %1").arg(graph.info.label || "")
        }
        Label {
            visible: graph.shown >= 0 && graph.samples.length > 0
            text: graph.samples.length ? graph.samples[graph.samples.length - 1].mm.toFixed(2) + " mm" : ""
            font.family: "monospace"
        }
        Item { Layout.fillWidth: true }
        Button {
            text: backend.streaming ? qsTr("Stop live depth") : qsTr("Start live depth")
            enabled: backend.connected && (!backend.busy || backend.streaming)
            onClicked: backend.setStreaming(!backend.streaming)
        }
    }

    Canvas {
        id: canvas
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 120
        property color fg: palette.windowText
        property color accent: palette.highlight
        property color base: palette.base
        property color grid: palette.mid

        Timer {
            interval: 33
            repeat: true
            running: backend.streaming && graph.visible
            onTriggered: canvas.requestPaint()
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            const w = width, h = height, left = 44, top = 6, bottom = h - 18, right = w - 6
            const y = (mm) => top + (bottom - top) * Math.min(1, Math.max(0, mm / graph.maxMm))
            const now = Date.now()
            const x = (t) => right - (right - left) * (now - t) / graph.windowMs

            ctx.reset()
            ctx.fillStyle = base
            ctx.fillRect(0, 0, w, h)
            ctx.font = "11px sans-serif"
            ctx.textBaseline = "middle"

            // grid: 0.5 mm steps, 1 s steps
            ctx.strokeStyle = grid
            ctx.fillStyle = fg
            ctx.lineWidth = 1
            for (let mm = 0; mm <= graph.maxMm + 0.001; mm += 0.5) {
                ctx.beginPath(); ctx.moveTo(left, y(mm)); ctx.lineTo(right, y(mm)); ctx.stroke()
                ctx.fillText(mm.toFixed(1), 6, y(mm))
            }
            for (let s = 0; s <= graph.windowMs / 1000; ++s) {
                const gx = right - (right - left) * s * 1000 / graph.windowMs
                ctx.beginPath(); ctx.moveTo(gx, top); ctx.lineTo(gx, bottom); ctx.stroke()
                const label = s === 0 ? qsTr("now") : "−" + s + " s"
                ctx.fillText(label, Math.min(gx - 12, right - ctx.measureText(label).width), h - 8)
            }

            if (graph.shown < 0) {
                ctx.fillText(backend.streaming ? qsTr("Press a key (or select one above)") : qsTr("Start live depth to see key travel"),
                             left + 12, (top + bottom) / 2)
                return
            }
            if (!backend.streaming && graph.samples.length === 0)
                ctx.fillText(qsTr("Start live depth to see key travel"), left + 12, (top + bottom) / 2)

            // shaded while the keyboard reports the key pressed
            const pts = graph.samples
            ctx.fillStyle = Qt.rgba(accent.r, accent.g, accent.b, 0.18)
            for (let i = 0; i < pts.length; ++i) {
                if (!pts[i].pressed) continue
                const x0 = Math.max(left, x(pts[i].t))
                const x1 = i + 1 < pts.length ? x(pts[i + 1].t) : right
                if (x1 > x0) ctx.fillRect(x0, top, x1 - x0, bottom - top)
            }

            // actuation point
            if (graph.info.actuation !== undefined) {
                ctx.strokeStyle = accent
                ctx.setLineDash([6, 4])
                ctx.beginPath(); ctx.moveTo(left, y(graph.info.actuation)); ctx.lineTo(right, y(graph.info.actuation)); ctx.stroke()
                ctx.setLineDash([])
                ctx.fillStyle = accent
                ctx.fillText(qsTr("actuation %1 mm").arg(graph.info.actuation.toFixed(2))
                             + (graph.info.mode > 0 ? qsTr(", RT ↓%1 ↑%2").arg(graph.info.rtPress.toFixed(2)).arg(graph.info.rtRelease.toFixed(2)) : ""),
                             left + 6, y(graph.info.actuation) - 8)
            }

            // travel (held between updates: the keyboard only sends changes)
            if (pts.length) {
                ctx.strokeStyle = fg
                ctx.lineWidth = 2
                ctx.beginPath()
                let first = true
                for (let i = 0; i < pts.length; ++i) {
                    const px = Math.max(left, x(pts[i].t))
                    if (first) { ctx.moveTo(px, y(pts[i].mm)); first = false }
                    else { ctx.lineTo(px, y(pts[i - 1].mm)); ctx.lineTo(px, y(pts[i].mm)) }
                }
                ctx.lineTo(right, y(pts[pts.length - 1].mm))
                ctx.stroke()
            }
        }
    }
}
