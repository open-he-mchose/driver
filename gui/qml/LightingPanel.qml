// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    function load() {
        effect.currentIndex = backend.lightEffect < backend.effectNames.length ? backend.lightEffect : 0
        brightness.value = backend.lightBrightness
        speed.value = backend.lightSpeed
        colorField.text = backend.lightColor
    }
    Component.onCompleted: load()
    Connections {
        target: backend
        function onSnapshotChanged() { panel.load() }
    }

    GridLayout {
        columns: 3
        columnSpacing: 16
        rowSpacing: 0
        width: Math.min(parent.width, 720)

        Label { text: qsTr("Effect") }
        ComboBox { id: effect; model: backend.effectNames; Layout.fillWidth: true }
        Item {}

        Label { text: qsTr("Brightness") }
        Slider { id: brightness; from: 0; to: 100; stepSize: 1; Layout.fillWidth: true }
        Label { text: brightness.value.toFixed(0) + " %"; Layout.preferredWidth: 60 }

        Label { text: qsTr("Speed") }
        Slider { id: speed; from: 0; to: 4; stepSize: 1; snapMode: Slider.SnapAlways; Layout.fillWidth: true }
        Label { text: speed.value.toFixed(0) }

        Label { text: qsTr("Colour") }
        TextField { id: colorField; placeholderText: "#rrggbb"; Layout.fillWidth: true }
        Rectangle { width: 28; height: 28; radius: 4; color: /^#[0-9a-fA-F]{6}$/.test(colorField.text) ? colorField.text : "transparent"; border.color: palette.mid }

        Item {}
        Button {
            text: qsTr("Apply effect")
            highlighted: true
            enabled: backend.connected && !backend.busy && /^#[0-9a-fA-F]{6}$/.test(colorField.text)
            onClicked: backend.applyLight(effect.currentIndex, brightness.value, speed.value, colorField.text)
        }
        Item {}

        Label { text: qsTr("Key colours"); font.bold: true; Layout.topMargin: 10; Layout.columnSpan: 3 }
        Label { text: qsTr("Colour") }
        TextField { id: paintColor; text: "#ff0000"; placeholderText: "#rrggbb"; Layout.fillWidth: true }
        Rectangle { width: 28; height: 28; radius: 4; color: /^#[0-9a-fA-F]{6}$/.test(paintColor.text) ? paintColor.text : "transparent"; border.color: palette.mid }
        Item {}
        Button {
            text: qsTr("Paint %n selected key(s)", "", backend.keys.selectedCount)
            enabled: backend.connected && !backend.busy && backend.keys.selectedCount > 0 && /^#[0-9a-fA-F]{6}$/.test(paintColor.text)
            onClicked: backend.paintSelected(paintColor.text)
        }
        Label {
            text: qsTr("shown with the “static” effect")
            opacity: 0.7
        }
    }
}
