// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    property var current: ({})
    Connections {
        target: backend.keys
        function onSelectionChanged() { panel.current = backend.keys.firstSelected(); code.text = panel.current.mapping || "" }
    }

    ColumnLayout {
        width: Math.min(parent.width, 720)
        spacing: 8
        Label {
            text: backend.keys.selectedCount === 1
                  ? qsTr("Key %1 is mapped to: %2 (layer 0 of the active profile)").arg(panel.current.label).arg(panel.current.mapping)
                  : qsTr("Select one key to remap it")
        }
        RowLayout {
            TextField {
                id: code
                Layout.fillWidth: true
                placeholderText: qsTr("Esc, Ctrl+Shift+T, Mute, F13, none, or raw hex 10 00 29")
                onAccepted: apply.clicked()
            }
            Button {
                id: apply
                text: qsTr("Remap")
                highlighted: true
                enabled: backend.connected && !backend.busy && backend.keys.selectedCount === 1 && code.text.length > 0
                onClicked: backend.remapSelected(code.text)
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: ["Esc", "LCtrl", "Backspace", "Mute", "VolumeUp", "VolumeDown", "PlayPause", "NextTrack", "PrevTrack", "none"]
                delegate: Button {
                    required property string modelData
                    text: modelData
                    flat: true
                    onClicked: code.text = modelData
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: qsTr("Macros and advanced keys (DKS, Mod-Tap, Toggle, Snap Tap …) are available in mchosectl for now; editors follow in a later version.")
        }
    }
}
