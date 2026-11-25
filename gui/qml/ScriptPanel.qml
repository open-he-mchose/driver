// SPDX-License-Identifier: GPL-3.0-only
// Lua lighting effects: examples, editor, run / stop, parameters, output.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Pane {
    id: panel
    objectName: "scriptPanel"
    property string scriptName: "effect.lua"
    property var paramValues: ({})   // values set by the user, applied on Run
    property string exampleText: ""   // the unchanged example of that name (not saved, so updates reach it)

    // Parameter values are saved per script name and come back when that script is loaded again.
    function setSource(text, name) {
        panel.exampleText = backend.exampleSource(name)
        editor.text = text
        panel.scriptName = name
        panel.paramValues = backend.savedParams(name)
        backend.setUiValue("scriptName", name)
        if (!backend.scriptRunning)
            backend.previewParams(text, panel.paramValues)
    }
    function loadExample(name) { setSource(backend.exampleSource(name), name) }
    function setParam(name, value) {
        panel.paramValues[name] = value
        if (backend.scriptRunning)
            backend.setScriptParam(name, value)   // saved under the running script's name
        else
            backend.saveParams(panel.scriptName, panel.paramValues)
    }
    Component.onCompleted: {
        // The editor as it was left (the running effect itself is started again by the backend on connect).
        const saved = backend.uiValue("scriptSource", "")
        const name = backend.uiValue("scriptName", "")
        if (saved.length)
            setSource(saved, name || "effect.lua")
        else if (backend.scriptExamples.indexOf(name) >= 0)
            loadExample(name)
        else if (backend.scriptExamples.length > 0)
            loadExample(backend.scriptExamples[0])
    }

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
                    onClicked: { log.clear(); backend.runScript(editor.text, panel.scriptName, panel.paramValues) }
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
                    onTextChanged: backend.setUiValue("scriptSource", text === panel.exampleText ? "" : text)
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

        // --- parameters (scrolls when the script declares many)
        ColumnLayout {
            Layout.preferredWidth: 260
            Layout.maximumWidth: 260
            Layout.fillHeight: true
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Parameters"); font.bold: true; Layout.fillWidth: true }
                Button {
                    text: qsTr("Defaults")
                    flat: true
                    visible: backend.scriptParams.length > 0
                    onClicked: {
                        if (backend.scriptRunning)
                            for (const p of backend.scriptParams)
                                backend.setScriptParam(p.name, p.def)
                        panel.paramValues = ({})
                        backend.saveParams(panel.scriptName, panel.paramValues)
                        backend.previewParams(editor.text)   // sliders back to the defaults
                    }
                }
            }
            Label {
                visible: backend.scriptParams.length === 0
                text: qsTr("This script declares no params.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                opacity: 0.7
            }
            ListView {
                id: paramList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                model: backend.scriptParams
                ScrollBar.vertical: ScrollBar { policy: paramList.contentHeight > paramList.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded }
                delegate: ColumnLayout {
                    id: row
                    required property var modelData
                    readonly property bool isSwitch: modelData.step === 1 && modelData.min === 0 && modelData.max === 1
                    readonly property int decimals: modelData.step >= 1 ? 0 : (modelData.max - modelData.min) >= 20 ? 0 : 2
                    width: paramList.width - (paramList.ScrollBar.vertical.visible ? paramList.ScrollBar.vertical.width + 4 : 0)
                    spacing: 0
                    ToolTip.visible: hover.hovered && labelText.truncated
                    ToolTip.text: modelData.label
                    HoverHandler { id: hover }

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            id: labelText
                            text: row.modelData.label
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: !row.isSwitch
                            text: slider.value.toFixed(row.decimals)
                            font.family: "monospace"
                            opacity: 0.8
                        }
                        Switch {
                            visible: row.isSwitch
                            checked: row.modelData.value >= 0.5
                            padding: 0
                            onToggled: panel.setParam(row.modelData.name, checked ? 1 : 0)
                        }
                    }
                    Slider {
                        id: slider
                        visible: !row.isSwitch
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight * 0.8
                        padding: 0
                        from: row.modelData.min
                        to: row.modelData.max
                        stepSize: row.modelData.step
                        snapMode: row.modelData.step > 0 ? Slider.SnapAlways : Slider.NoSnap
                        value: row.modelData.value
                        onMoved: panel.setParam(row.modelData.name, value)
                    }
                }
            }
        }
    }

    FileDialog {
        id: openDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Lua scripts (*.lua)"]
        onAccepted: {
            const text = backend.readTextFile(selectedFile)
            if (text.length)
                panel.setSource(text, selectedFile.toString().split("/").pop())
        }
    }
    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["Lua scripts (*.lua)"]
        defaultSuffix: "lua"
        onAccepted: {
            if (backend.writeTextFile(selectedFile, editor.text)) {
                panel.scriptName = selectedFile.toString().split("/").pop()
                backend.setUiValue("scriptName", panel.scriptName)
                backend.saveParams(panel.scriptName, panel.paramValues)
            }
        }
    }
}
