// SPDX-License-Identifier: GPL-3.0-only
// Remapping without typing: select keys on the keyboard at the top, then pick the new function here.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    property var current: ({})
    readonly property bool canWrite: backend.connected && !backend.busy && backend.keys.selectedCount > 0
    Connections {
        target: backend.keys
        function onSelectionChanged() { panel.current = backend.keys.firstSelected() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 16

        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.alignment: Qt.AlignTop
            spacing: 6
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: backend.keys.selectedCount === 0
                      ? qsTr("1. Select the key(s) to change on the keyboard at the top.\n2. Click the new function on the small keyboard, or use a button below.")
                      : backend.keys.selectedCount === 1
                        ? (panel.current.remapped ? qsTr("%1 → %2 (changed)").arg(panel.current.label).arg(panel.current.mapping)
                                                  : qsTr("%1: default function (%2)").arg(panel.current.label).arg(panel.current.mapping))
                        : qsTr("%n keys selected", "", backend.keys.selectedCount)
            }
            RowLayout {
                Button {
                    text: qsTr("Default")
                    enabled: panel.canWrite
                    onClicked: backend.resetSelected()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Restore the factory function (read from the keyboard's firmware)")
                }
                Button {
                    text: qsTr("Disable")
                    enabled: panel.canWrite
                    onClicked: backend.remapSelected("none")
                }
            }
            Label { text: qsTr("Media"); font.bold: true; Layout.topMargin: 6 }
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: [["Mute", "Mute"], ["Vol −", "VolumeDown"], ["Vol +", "VolumeUp"], ["Play/Pause", "PlayPause"],
                            ["Prev", "PrevTrack"], ["Next", "NextTrack"], ["Stop", "Stop"]]
                    delegate: Button {
                        required property var modelData
                        text: modelData[0]
                        enabled: panel.canWrite
                        onClicked: backend.remapSelected(modelData[1])
                    }
                }
            }
            Label { text: qsTr("Other code"); font.bold: true; Layout.topMargin: 6 }
            RowLayout {
                TextField {
                    id: code
                    Layout.fillWidth: true
                    placeholderText: qsTr("Ctrl+Shift+T, F13, raw 10 00 29")
                    onAccepted: if (panel.canWrite && text.length) backend.remapSelected(text)
                }
                Button {
                    text: qsTr("Set")
                    enabled: panel.canWrite && code.text.length > 0
                    onClicked: backend.remapSelected(code.text)
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Label {
                text: qsTr("Click a key below to give the selected key(s) its function:")
                opacity: panel.canWrite ? 1 : 0.5
            }
            KeyboardView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                picker: true
                base: ApplicationWindow.window ? ApplicationWindow.window.color : palette.window
                overlay: "none"
                enabled: panel.canWrite
                opacity: panel.canWrite ? 1 : 0.5
                onPicked: (matrixIndex) => backend.assignFactoryOf(matrixIndex)
            }
        }
    }
}
