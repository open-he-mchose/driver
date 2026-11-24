// SPDX-License-Identifier: GPL-3.0-only
// Lua lighting effects: examples, editor, run / stop, parameters, output.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Pane {
    id: panel
    property string scriptName: "effect.lua"
    property var paramValues: ({})

    function loadExample(name) {
        editor.text = backend.exampleSource(name)
        panel.scriptName = name
    }
    Component.onCompleted: if (backend.scriptExamples.length > 0) loadExample(backend.scriptExamples[0])

    Connections {
        target: backend
        function onScriptLog(line, error) {
            log.append((error ? "⚠ " : "") + line)
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 10

        // --- examples and files
        ColumnLayout {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            Label { text: qsTr("Examples"); font.bold: true }
            ListView {
                id: examples
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: backend.scriptExamples
                delegate: ItemDelegate {
                    required property string modelData
                    width: ListView.view.width
                    text: modelData.replace(".lua", "").replace(/_/g, " ")
                    highlighted: panel.scriptName === modelData
                    onClicked: panel.loadExample(modelData)
                }
            }
            Button { text: qsTr("Open…"); Layout.fillWidth: true; onClicked: openDialog.open() }
            Button { text: qsTr("Save as…"); Layout.fillWidth: true; onClicked: saveDialog.open() }
        }

        // --- editor and output
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            RowLayout {
                Label { text: panel.scriptName; font.bold: true }
                Item { Layout.fillWidth: true }
                Button {
                    text: backend.scriptRunning ? qsTr("Restart") : qsTr("Run")
                    enabled: backend.connected
                    onClicked: { log.clear(); backend.runScript(editor.text, panel.scriptName) }
                }
                Button { text: qsTr("Stop"); enabled: backend.scriptRunning; onClicked: backend.stopScript() }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 3
                TextArea {
                    id: editor
                    font.family: "monospace"
                    wrapMode: TextEdit.NoWrap
                    tabStopDistance: 4 * fontMetrics.averageCharacterWidth
                    selectByMouse: true
                    FontMetrics { id: fontMetrics; font: editor.font }
                }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                TextArea {
                    id: log
                    readOnly: true
                    font.family: "monospace"
                    wrapMode: TextEdit.Wrap
                    placeholderText: qsTr("Output: print() from the script and errors. Effects use live colours only (no flash writes); key presses come from the system.")
                }
            }
        }

        // --- parameters
        ColumnLayout {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            Label { text: qsTr("Parameters"); font.bold: true }
            Label {
                visible: backend.scriptParams.length === 0
                text: qsTr("Run a script that declares params to tune it here.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                opacity: 0.7
            }
            Repeater {
                model: backend.scriptParams
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 0
                    Label { text: modelData.label + ": " + slider.value.toFixed(2) }
                    Slider {
                        id: slider
                        Layout.fillWidth: true
                        from: modelData.min
                        to: modelData.max
                        value: modelData.value
                        onMoved: backend.setScriptParam(modelData.name, value)
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }

    FileDialog {
        id: openDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Lua scripts (*.lua)"]
        onAccepted: {
            const text = backend.readTextFile(selectedFile)
            if (text.length) {
                editor.text = text
                panel.scriptName = selectedFile.toString().split("/").pop()
            }
        }
    }
    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["Lua scripts (*.lua)"]
        defaultSuffix: "lua"
        onAccepted: {
            if (backend.writeTextFile(selectedFile, editor.text))
                panel.scriptName = selectedFile.toString().split("/").pop()
        }
    }
}
