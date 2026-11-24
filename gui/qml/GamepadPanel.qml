// SPDX-License-Identifier: GPL-3.0-only
// Analog keys as a virtual Xbox 360 controller: mapping (text), dead zone / full press / button point, live view.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    property var check: ({ ok: true, count: 0 })
    readonly property bool canWrite: backend.connected && !backend.busy

    function setText(t) { editor.text = t }
    function recheck() { panel.check = backend.gamepadCheck(editor.text) }
    Component.onCompleted: {
        editor.text = backend.gamepadPreset("wasd-left-stick")
        recheck()
    }
    Connections {
        target: backend
        function onSnapshotChanged() { panel.recheck() }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        // --- presets, mapping of selected keys, curve
        ScrollView {
            Layout.preferredWidth: 290
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 4

                Label { text: qsTr("Preset"); font.bold: true }
                RowLayout {
                    ComboBox { id: preset; model: backend.gamepadPresets; currentIndex: Math.max(0, backend.gamepadPresets.indexOf("wasd-left-stick")); Layout.fillWidth: true }
                    Button { text: qsTr("Load"); onClicked: panel.setText(backend.gamepadPreset(preset.currentText)) }
                }

                Label { text: qsTr("Selected key(s) on the keyboard"); font.bold: true; Layout.topMargin: 8 }
                RowLayout {
                    ComboBox { id: target; model: backend.gamepadTargets; Layout.fillWidth: true }
                    Button {
                        text: qsTr("Map")
                        enabled: backend.keys.selectedCount > 0
                        onClicked: panel.setText(backend.gamepadBindSelected(editor.text, target.currentText))
                    }
                }
                Button {
                    text: qsTr("Remove from the gamepad")
                    enabled: backend.keys.selectedCount > 0
                    onClicked: panel.setText(backend.gamepadUnbindSelected(editor.text))
                }

                Label { text: qsTr("Travel"); font.bold: true; Layout.topMargin: 8 }
                Repeater {
                    model: [["deadzone", qsTr("Dead zone (ignored at the top)")],
                            ["full", qsTr("Full analog value at")],
                            ["button", qsTr("Buttons press at")]]
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            Label { text: modelData[1]; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: curve.value.toFixed(2) + " mm"; font.family: "monospace" }
                        }
                        Slider {
                            id: curve
                            Layout.fillWidth: true
                            from: 0; to: 3.4; stepSize: 0.05
                            value: panel.check[modelData[0]] !== undefined ? panel.check[modelData[0]] : 0
                            onMoved: panel.setText(backend.gamepadSetCurve(editor.text, modelData[0], value))
                        }
                    }
                }

                Label { text: qsTr("Typing"); font.bold: true; Layout.topMargin: 8 }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.8
                    text: qsTr("Mapped keys still type on the keyboard. For games, disable them in the active profile (one flash write) and restore them afterwards — or keep a separate gaming profile.")
                }
                RowLayout {
                    Button {
                        text: qsTr("Disable mapped keys")
                        enabled: panel.canWrite && panel.check.ok && panel.check.count > 0
                        onClicked: backend.gamepadKeysTyping(editor.text, false)
                    }
                    Button {
                        text: qsTr("Restore")
                        enabled: panel.canWrite && panel.check.ok && panel.check.count > 0
                        onClicked: backend.gamepadKeysTyping(editor.text, true)
                    }
                }
            }
        }

        // --- mapping text
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            Label { text: qsTr("Mapping (KEY = TARGET)"); font.bold: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: editor
                    font.family: "monospace"
                    wrapMode: TextEdit.NoWrap
                    selectByMouse: true
                    onTextChanged: panel.recheck()
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.bold: !panel.check.ok
                text: panel.check.ok ? qsTr("%n key(s) mapped. Targets: %1", "", panel.check.count).arg(backend.gamepadTargets.join(", "))
                                     : "⚠ " + panel.check.error
                opacity: panel.check.ok ? 0.7 : 1
            }
        }

        // --- run and live view
        ColumnLayout {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            spacing: 6
            Button {
                Layout.fillWidth: true
                text: backend.gamepadRunning ? qsTr("Stop gamepad") : qsTr("Start gamepad")
                highlighted: !backend.gamepadRunning
                enabled: backend.connected && (backend.gamepadRunning || (panel.check.ok && panel.check.count > 0))
                onClicked: backend.gamepadRunning ? backend.stopGamepad() : backend.startGamepad(editor.text)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.8
                text: backend.gamepadStatus.length ? backend.gamepadStatus
                      : qsTr("Uses the live depth stream; Lighting effects are stopped while the gamepad runs.")
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16
                Repeater {
                    model: [["lx", "ly", qsTr("Left")], ["rx", "ry", qsTr("Right")]]
                    delegate: ColumnLayout {
                        required property var modelData
                        spacing: 2
                        Rectangle {
                            id: ring
                            Layout.preferredWidth: 90
                            Layout.preferredHeight: 90
                            radius: 45
                            color: palette.base
                            border.color: palette.mid
                            Rectangle {
                                readonly property real vx: backend.gamepadLive[modelData[0]] || 0
                                readonly property real vy: backend.gamepadLive[modelData[1]] || 0
                                width: 14; height: 14; radius: 7
                                color: palette.highlight
                                x: ring.width / 2 - 7 + vx * (ring.width / 2 - 7)
                                y: ring.height / 2 - 7 - vy * (ring.height / 2 - 7)
                            }
                        }
                        Label { text: modelData[2]; Layout.alignment: Qt.AlignHCenter }
                    }
                }
            }
            Repeater {
                model: [["lt", "LT"], ["rt", "RT"]]
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { text: modelData[1]; Layout.preferredWidth: 24 }
                    ProgressBar { from: 0; to: 1; value: backend.gamepadLive[modelData[0]] || 0; Layout.fillWidth: true }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Buttons: %1").arg((backend.gamepadLive.buttons || []).length ? backend.gamepadLive.buttons.join(" ") : "—")
            }
            Item { Layout.fillHeight: true }
        }
    }
}
