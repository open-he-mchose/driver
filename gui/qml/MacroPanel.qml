// SPDX-License-Identifier: GPL-3.0-only
// Macro editor: list of M1-M16, steps of the selected macro, add / record steps, bind to keys.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    property int current: 0
    readonly property var editor: backend.macroEditor
    Component.onCompleted: backend.editMacro(current)
    Connections {
        target: backend
        function onSnapshotChanged() { if (!panel.editor.modified) backend.editMacro(panel.current) }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        // --- M1..M16
        ListView {
            id: list
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            clip: true
            model: backend.macros
            currentIndex: panel.current
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property int index
                required property var modelData
                width: ListView.view.width
                highlighted: index === panel.current
                text: "M" + (index + 1) + "  " + (modelData.steps ? qsTr("%n step(s)", "", modelData.steps) : qsTr("empty"))
                      + (modelData.bound ? "\n    " + modelData.bound : "")
                onClicked: { panel.current = index; backend.editMacro(index) }
            }
        }

        // --- steps
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            RowLayout {
                Label { text: qsTr("M%1 steps").arg(panel.current + 1) + (panel.editor.modified ? qsTr(" (not saved)") : ""); font.bold: true }
                Item { Layout.fillWidth: true }
                Button { text: qsTr("Save"); enabled: panel.editor.modified && backend.connected && !backend.busy; onClicked: backend.saveMacro(panel.current) }
                Button { text: qsTr("Revert"); enabled: panel.editor.modified; onClicked: backend.editMacro(panel.current) }
                Button { text: qsTr("Delete macro"); enabled: backend.connected && !backend.busy; onClicked: backend.clearMacro(panel.current) }
            }
            ListView {
                id: steps
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: panel.editor
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    id: row
                    required property int index
                    required property string kind
                    required property string name
                    required property bool press
                    required property int delay
                    width: ListView.view.width
                    spacing: 4
                    Label { text: (row.index + 1) + "."; Layout.preferredWidth: 28; horizontalAlignment: Text.AlignRight }
                    ComboBox {
                        visible: row.kind === "key"
                        model: [qsTr("press"), qsTr("release")]
                        currentIndex: row.press ? 0 : 1
                        onActivated: (i) => panel.editor.setPress(row.index, i === 0)
                        Layout.preferredWidth: 110
                    }
                    ComboBox {
                        visible: row.kind === "key"
                        editable: true
                        model: panel.editor.keyNames
                        currentIndex: Math.max(0, panel.editor.keyNames.indexOf(row.name))
                        editText: row.name
                        onAccepted: panel.editor.setKey(row.index, editText)
                        onActivated: panel.editor.setKey(row.index, currentText)
                        Layout.preferredWidth: 140
                    }
                    Label { visible: row.kind !== "key"; text: row.kind === "delay" ? qsTr("pause") : row.name; Layout.preferredWidth: 254 }
                    Label { text: qsTr("then wait") }
                    SpinBox {
                        from: 0; to: 65535; stepSize: 10
                        value: row.delay
                        editable: true
                        onValueModified: panel.editor.setDelay(row.index, value)
                        Layout.preferredWidth: 130
                    }
                    Label { text: qsTr("ms") }
                    Item { Layout.fillWidth: true }
                    ToolButton { text: "↑"; enabled: row.index > 0; onClicked: panel.editor.move(row.index, -1) }
                    ToolButton { text: "↓"; enabled: row.index < steps.count - 1; onClicked: panel.editor.move(row.index, 1) }
                    ToolButton { text: "✕"; onClicked: panel.editor.remove(row.index) }
                }
                Label {
                    anchors.centerIn: parent
                    visible: steps.count === 0
                    text: qsTr("No steps. Add keys on the right or record them.")
                    opacity: 0.7
                }
            }
        }

        // --- add / record / bind
        ScrollView {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 4

                Label { text: qsTr("Add"); font.bold: true }
                RowLayout {
                    ComboBox { id: addKey; editable: true; model: panel.editor.keyNames; Layout.fillWidth: true }
                    Button { text: qsTr("Tap"); onClicked: panel.editor.addTap(addKey.editText) }
                }
                RowLayout {
                    Button { text: qsTr("Press"); onClicked: panel.editor.addPress(addKey.editText, true) }
                    Button { text: qsTr("Release"); onClicked: panel.editor.addPress(addKey.editText, false) }
                }
                RowLayout {
                    SpinBox { id: pause; from: 1; to: 65535; value: 100; stepSize: 50; editable: true; Layout.fillWidth: true }
                    Button { text: qsTr("Pause (ms)"); onClicked: panel.editor.addDelay(pause.value) }
                }
                RowLayout {
                    TextField { id: typed; placeholderText: qsTr("text to type"); Layout.fillWidth: true }
                    Button { text: qsTr("Type"); enabled: typed.text.length > 0; onClicked: { panel.editor.addText(typed.text); typed.text = "" } }
                }
                RowLayout {
                    Label { text: qsTr("Delay between new steps") }
                    SpinBox {
                        from: 0; to: 1000; stepSize: 5
                        value: panel.editor.defaultDelay
                        onValueModified: panel.editor.defaultDelay = value
                        Layout.preferredWidth: 110
                    }
                }

                Label { text: qsTr("Record"); font.bold: true; Layout.topMargin: 8 }
                RowLayout {
                    Button {
                        text: panel.editor.recording ? qsTr("Stop") : qsTr("Record keys")
                        highlighted: panel.editor.recording
                        onClicked: {
                            if (panel.editor.recording) panel.editor.stopRecording()
                            else { panel.editor.startRecording(realDelays.checked); recorder.forceActiveFocus() }
                        }
                    }
                    CheckBox { id: realDelays; text: qsTr("real delays"); checked: true }
                }
                Label {
                    visible: panel.editor.recording
                    text: qsTr("Type now; click Stop when done.")
                    font.bold: true
                }
                Item {
                    id: recorder
                    width: 1; height: 1
                    Keys.onPressed: (e) => { e.accepted = panel.editor.recordKey(e.key, true, e.isAutoRepeat) }
                    Keys.onReleased: (e) => { e.accepted = panel.editor.recordKey(e.key, false, e.isAutoRepeat) }
                }

                Label { text: qsTr("Bind to the selected key(s)"); font.bold: true; Layout.topMargin: 8 }
                RowLayout {
                    ComboBox {
                        id: mode
                        model: [qsTr("once"), qsTr("while held"), qsTr("toggle"), qsTr("N times")]
                        Layout.fillWidth: true
                    }
                    SpinBox { id: times; visible: mode.currentIndex === 3; from: 2; to: 254; value: 3; editable: true; Layout.preferredWidth: 100 }
                }
                Button {
                    text: qsTr("Bind M%1 (layer: %2)").arg(panel.current + 1).arg(["Base", "Fn", "Mac", "Mac Fn"][backend.keys.layer])
                    enabled: backend.connected && !backend.busy && backend.keys.selectedCount > 0 && !panel.editor.modified
                    onClicked: backend.bindMacro(panel.current, [1, 0, 255, times.value][mode.currentIndex])
                }

                Label { text: qsTr("Bound to"); font.bold: true; Layout.topMargin: 8 }
                Label {
                    readonly property string bound: backend.macros.length > panel.current ? backend.macros[panel.current].bound : ""
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: bound.length ? bound : qsTr("no keys")
                    opacity: bound.length ? 1 : 0.7
                }
                Button {
                    text: qsTr("Remove M%1 from its keys").arg(panel.current + 1)
                    enabled: backend.connected && !backend.busy && backend.macros.length > panel.current
                             && backend.macros[panel.current].bound.length > 0
                    onClicked: backend.unbindMacro(panel.current)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("The keys get their default function back (all layers of the active profile)")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("Macros are shared by all profiles; bindings belong to the active profile. Delete macro also frees its keys. On Linux macro keys need the NKRO fix (linux-fixes).")
                }
            }
        }
    }
}
