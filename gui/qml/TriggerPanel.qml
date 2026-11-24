// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    function load() {
        const k = backend.keys.firstSelected()
        if (k.label === undefined) return
        act.value = k.actuation
        rt.currentIndex = k.mode
        press.value = k.rtPress
        release.value = k.rtRelease
    }
    Connections {
        target: backend.keys
        function onSelectionChanged() {
            panel.load()
            depthGraph.tracked = backend.keys.selectedCount > 0 ? backend.keys.firstSelected().matrixIndex : -1
        }
    }

    RowLayout {
    anchors.fill: parent
    spacing: 16

    GridLayout {
        columns: 3
        columnSpacing: 16
        rowSpacing: 0
        Layout.preferredWidth: 520
        Layout.maximumWidth: 600
        Layout.alignment: Qt.AlignTop

        Label { text: qsTr("Actuation point") }
        Slider { id: act; from: 0.1; to: 3.4; stepSize: 0.01; value: 1.5; Layout.fillWidth: true }
        Label { text: act.value.toFixed(2) + " mm"; Layout.preferredWidth: 70 }

        Label { text: qsTr("Rapid trigger"); ToolTip.visible: rtHover.hovered; ToolTip.text: qsTr("Keys with rapid trigger show their actuation in the accent colour"); HoverHandler { id: rtHover } }
        ComboBox {
            id: rt
            model: [qsTr("Off"), qsTr("On"), qsTr("Continuous")]
            currentIndex: 1
            Layout.fillWidth: true
        }
        Item {}

        Label { text: qsTr("RT press distance"); enabled: rt.currentIndex > 0 }
        Slider { id: press; from: 0.01; to: 2.0; stepSize: 0.01; value: 0.3; Layout.fillWidth: true; enabled: rt.currentIndex > 0 }
        Label { text: press.value.toFixed(2) + " mm"; enabled: rt.currentIndex > 0 }

        Label { text: qsTr("RT release distance"); enabled: rt.currentIndex > 0 }
        Slider { id: release; from: 0.01; to: 2.0; stepSize: 0.01; value: 0.3; Layout.fillWidth: true; enabled: rt.currentIndex > 0 }
        Label { text: release.value.toFixed(2) + " mm"; enabled: rt.currentIndex > 0 }

        Item {}
        Button {
            text: qsTr("Apply to %n selected key(s)", "", backend.keys.selectedCount)
            enabled: backend.connected && !backend.busy && backend.keys.selectedCount > 0
            highlighted: true
            onClicked: backend.applyTrigger(act.value, rt.currentIndex, press.value, release.value)
        }
        Item {}
    }

    DepthGraph {
        id: depthGraph
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
    }
}
