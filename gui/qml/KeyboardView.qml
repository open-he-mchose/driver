// SPDX-License-Identifier: GPL-3.0-only
// The physical keyboard. Click / Shift+click / drag to select keys; shows a value per key (overlay) and,
// while the depth stream runs, how deep every key is pressed.
import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string overlay: "actuation"   // actuation | mapping | color | depth | none
    property bool picker: false               // palette mode: clicking a key emits picked() instead of selecting
    signal picked(int matrixIndex)
    readonly property real u: Math.min((width - 8) / backend.keys.widthUnits, (height - 8) / backend.keys.heightUnits)
    readonly property real ox: (width - u * backend.keys.widthUnits) / 2   // centre horizontally
    readonly property real maxTravel: 3.4

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: palette.base
    }

    // Rubber-band selection on the empty area.
    MouseArea {
        id: band
        anchors.fill: parent
        enabled: !root.picker
        property point start
        property bool dragging: false
        onPressed: (m) => { start = Qt.point(m.x, m.y); dragging = true; rect.x = m.x; rect.y = m.y; rect.width = 0; rect.height = 0 }
        onPositionChanged: (m) => {
            rect.x = Math.min(start.x, m.x); rect.y = Math.min(start.y, m.y)
            rect.width = Math.abs(m.x - start.x); rect.height = Math.abs(m.y - start.y)
        }
        onReleased: (m) => {
            dragging = false
            const add = (m.modifiers & Qt.ShiftModifier) || (m.modifiers & Qt.ControlModifier)
            if (rect.width < 4 && rect.height < 4) { if (!add) backend.keys.clearSelection(); return }
            backend.keys.selectRect((rect.x - root.ox) / root.u, (rect.y - 4) / root.u,
                                    (rect.x + rect.width - root.ox) / root.u, (rect.y + rect.height - 4) / root.u, add)
        }
    }
    Rectangle {
        id: rect
        visible: band.dragging
        color: "transparent"
        border.color: palette.highlight
    }

    Repeater {
        model: backend.keys
        delegate: Item {
            id: key
            required property int index
            required property int matrixIndex
            required property string label
            required property real kx
            required property real ky
            required property real kw
            required property string mapping
            required property real actuation
            required property int mode
            required property string keyColor
            required property real depth
            required property bool pressed
            required property bool selected
            required property bool remapped
            required property bool dangerous
            required property string liveColor

            x: root.ox + kx * root.u + 2
            y: 4 + ky * root.u + 2
            width: kw * root.u - 4
            height: root.u - 4

            Rectangle {
                id: cap
                anchors.fill: parent
                radius: 6
                color: key.pressed ? palette.midlight : palette.button
                border.width: key.selected && !root.picker ? 2 : 1
                border.color: key.selected && !root.picker ? palette.highlight : palette.mid
                clip: true

                // Depth fill (live stream).
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: parent.height * Math.min(1, key.depth / root.maxTravel)
                    color: palette.highlight
                    opacity: 0.55
                    visible: key.depth > 0 && !root.picker
                }
                // Live preview of a running effect (the effect's own colours).
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: 4
                    visible: key.liveColor.length > 0 && !root.picker
                    color: key.liveColor.length > 0 ? key.liveColor : "transparent"
                    opacity: 0.85
                }
                // Stored colour swatch (lighting tab).
                Rectangle {
                    visible: root.overlay === "color"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 4
                    width: 10; height: 10; radius: 5
                    color: key.keyColor
                    border.color: palette.mid
                }
            }
            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 5
                text: key.label
                color: palette.buttonText
                font.pixelSize: Math.max(9, root.u * 0.22)
            }
            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                font.pixelSize: Math.max(8, root.u * 0.18)
                color: (root.overlay === "mapping" && key.remapped) || (root.overlay === "actuation" && key.mode > 0)
                       ? palette.highlight : palette.placeholderText
                text: root.overlay === "actuation" ? key.actuation.toFixed(2)
                    : root.overlay === "mapping" ? ((key.dangerous ? "⚠ " : "") + (key.remapped || key.dangerous || backend.keys.layer > 0 ? key.mapping : ""))
                    : root.overlay === "depth" ? (key.depth > 0 ? key.depth.toFixed(2) : "")
                    : ""
            }
            ToolTip.visible: tipArea.containsMouse && !root.picker && root.overlay === "mapping" && key.mapping.length > 0
            ToolTip.text: key.mapping
            ToolTip.delay: 400
            MouseArea {
                id: tipArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: root.picker ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: (m) => {
                    if (root.picker) root.picked(key.matrixIndex)
                    else backend.keys.select(key.index, (m.modifiers & Qt.ShiftModifier) || (m.modifiers & Qt.ControlModifier))
                }
            }
        }
    }
}
