// SPDX-License-Identifier: GPL-3.0-only
// Advanced keys: list of the profile's bindings (with problems), creation of new ones on the selected keys.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel
    readonly property bool canWrite: backend.connected && !backend.busy
    readonly property var keyNames: backend.macroEditor.keyNames
    readonly property string layerName: ["Base", "Fn", "Mac", "Mac Fn"][backend.keys.layer]

    RowLayout {
        anchors.fill: parent
        spacing: 12

        // --- existing bindings
        ColumnLayout {
            Layout.preferredWidth: 420
            Layout.minimumWidth: 300
            Layout.fillHeight: true
            spacing: 4
            Label {
                text: qsTr("Advanced keys of profile %1: %2").arg(backend.activeProfile).arg(backend.advanced.length)
                font.bold: true
            }
            ListView {
                id: bindings
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: backend.advanced
                spacing: 4
                ScrollBar.vertical: ScrollBar {}
                delegate: Frame {
                    required property var modelData
                    width: ListView.view.width
                    padding: 6
                    RowLayout {
                        anchors.fill: parent
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label { text: modelData.label + " — " + modelData.kind; font.bold: true }
                            Label { text: modelData.summary; wrapMode: Text.WordWrap; Layout.fillWidth: true; opacity: 0.85 }
                            Label {
                                visible: modelData.problem.length > 0
                                text: "⚠ " + modelData.problem
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                font.bold: true
                            }
                        }
                        Button {
                            text: qsTr("Remove")
                            enabled: panel.canWrite
                            onClicked: backend.removeAdvanced(modelData.layer, modelData.key)
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: bindings.count === 0
                    text: qsTr("No advanced keys in this profile.")
                    opacity: 0.7
                }
            }
            Label {
                visible: backend.advancedOrphans.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                text: qsTr("Unused table entries (harmless leftovers): %1").arg(backend.advancedOrphans.join(", "))
            }
        }

        // --- create
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 500   // do not let the forms' implicit width widen the window
            Layout.minimumWidth: 300
            Layout.fillHeight: true
            spacing: 4
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("New, on the selected key(s) in layer %1 (choose the layer on the Keymap tab):").arg(panel.layerName)
            }
            TabBar {
                id: kind
                Layout.fillWidth: true
                Layout.preferredWidth: 100
                Component.onCompleted: {   // screenshot mode: --sub N selects the form
                    const a = Qt.application.arguments, i = a.indexOf("--sub")
                    if (i >= 0 && i + 1 < a.length) currentIndex = parseInt(a[i + 1])
                }
                TabButton { text: qsTr("Snap Tap") }
                TabButton { text: qsTr("Rapid Strike") }
                TabButton { text: qsTr("LT") }
                TabButton { text: qsTr("Mod-Tap") }
                TabButton { text: qsTr("Toggle") }
                TabButton { text: qsTr("DKS") }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth
                StackLayout {
                    width: parent.width
                    currentIndex: kind.currentIndex <= 2 ? 0 : kind.currentIndex - 2

                    // Snap Tap / RS / LT
                    ColumnLayout {
                        spacing: 6
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: [qsTr("Snap Tap (SOCD): two keys that cancel each other, e.g. A and D."),
                                   qsTr("Rapid Strike: while both keys are pressed, the deeper one is active."),
                                   qsTr("LT: releasing one key taps the other (counter-strafe).")][Math.min(kind.currentIndex, 2)]
                        }
                        RowLayout {
                            visible: kind.currentIndex === 0
                            Label { text: qsTr("When both are pressed") }
                            ComboBox {
                                id: socdMode
                                model: [qsTr("last input wins"), qsTr("first key wins"), qsTr("second key wins"), qsTr("neither (cancel)")]
                                Layout.fillWidth: true
                            }
                        }
                        Button {
                            text: qsTr("Create for the 2 selected keys")
                            enabled: panel.canWrite && backend.keys.selectedCount === 2
                            onClicked: backend.createPair(kind.currentIndex, socdMode.currentIndex)
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.7
                            visible: kind.currentIndex === 0
                            text: qsTr("Snap Tap output needs the NKRO fix on Linux and does not reach Windows (firmware bug).")
                        }
                    }

                    // Mod-Tap
                    GridLayout {
                        columns: 2
                        rowSpacing: 4
                        Label { text: qsTr("Tap sends") }
                        ComboBox { id: tap; editable: true; model: panel.keyNames; editText: "Esc"; Layout.fillWidth: true }
                        Label { text: qsTr("Hold sends") }
                        ComboBox { id: hold; editable: true; model: panel.keyNames; editText: "LCtrl"; Layout.fillWidth: true }
                        Label { text: qsTr("Hold after (ms)") }
                        SpinBox { id: holdMs; from: 10; to: 2550; stepSize: 10; value: 200; editable: true }
                        Item {}
                        Button {
                            text: qsTr("Create for the selected key")
                            enabled: panel.canWrite && backend.keys.selectedCount === 1
                            onClicked: backend.createModTap(tap.editText, hold.editText, holdMs.value)
                        }
                    }

                    // Toggle
                    ColumnLayout {
                        spacing: 6
                        Label { text: qsTr("One press holds the key, the next press releases it.") }
                        RowLayout {
                            Label { text: qsTr("Key to hold") }
                            ComboBox { id: toggleKey; editable: true; model: [qsTr("(the key itself)")].concat(panel.keyNames); Layout.fillWidth: true }
                        }
                        Button {
                            text: qsTr("Create for the selected key")
                            enabled: panel.canWrite && backend.keys.selectedCount === 1
                            onClicked: backend.createToggle(toggleKey.currentIndex === 0 && toggleKey.editText === toggleKey.textAt(0) ? "" : toggleKey.editText)
                        }
                    }

                    // DKS
                    ColumnLayout {
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("Up to four actions by press phase. Each number says for how many phases the key stays held when it starts there (0 = not used).")
                        }
                        RowLayout {
                            Label { text: qsTr("Shallow point") }
                            SpinBox {
                                id: shallow; from: 1; to: 33; value: 10; editable: true
                                textFromValue: (v) => (v / 10).toFixed(1) + " mm"
                                valueFromText: (t) => Math.round(parseFloat(t) * 10)
                            }
                            Label { text: qsTr("Deep point") }
                            SpinBox {
                                id: deep; from: 2; to: 34; value: 30; editable: true
                                textFromValue: (v) => (v / 10).toFixed(1) + " mm"
                                valueFromText: (t) => Math.round(parseFloat(t) * 10)
                            }
                        }
                        RowLayout {   // column headers, same widths as the rows below
                            Label { text: qsTr("Key"); Layout.preferredWidth: 130 }
                            Label { text: qsTr("press ↓"); Layout.preferredWidth: 90; ToolTip.visible: h1.hovered; ToolTip.text: qsTr("starts when pressed past the shallow point (0-4)"); HoverHandler { id: h1 } }
                            Label { text: qsTr("bottom ↓"); Layout.preferredWidth: 90; ToolTip.visible: h2.hovered; ToolTip.text: qsTr("starts when pressed past the deep point (0-3)"); HoverHandler { id: h2 } }
                            Label { text: qsTr("lift ↑"); Layout.preferredWidth: 90; ToolTip.visible: h3.hovered; ToolTip.text: qsTr("starts when lifted from the deep point (0-2)"); HoverHandler { id: h3 } }
                            Label { text: qsTr("release ↑"); Layout.preferredWidth: 90; ToolTip.visible: h4.hovered; ToolTip.text: qsTr("starts when lifted past the shallow point (0-1)"); HoverHandler { id: h4 } }
                        }
                        Repeater {
                            id: rows
                            model: 4
                            delegate: RowLayout {
                                id: r
                                required property int index
                                property alias key: k.editText
                                property alias ds: s1.value
                                property alias de: s2.value
                                property alias us: s3.value
                                property alias ue: s4.value
                                readonly property var stored: backend.storedSpans(s1.value, s2.value, s3.value, s4.value)
                                ComboBox { id: k; editable: true; model: [""].concat(panel.keyNames); Layout.preferredWidth: 130 }
                                SpinBox { id: s1; from: 0; to: 4; value: r.index === 0 ? 1 : 0; Layout.preferredWidth: 90 }
                                SpinBox { id: s2; from: 0; to: 3; value: r.index === 1 ? 1 : 0; Layout.preferredWidth: 90 }
                                SpinBox { id: s3; from: 0; to: 2; value: r.index === 2 ? 1 : 0; Layout.preferredWidth: 90 }
                                SpinBox { id: s4; from: 0; to: 1; value: r.index === 3 ? 1 : 0; Layout.preferredWidth: 90 }
                                Label {
                                    visible: k.editText.length > 0 && (r.stored[0] !== s1.value || r.stored[1] !== s2.value || r.stored[2] !== s3.value || r.stored[3] !== s4.value)
                                    text: qsTr("stored as %1").arg(r.stored.join(","))
                                    font.bold: true
                                }
                            }
                        }
                        Button {
                            text: qsTr("Create for the selected key")
                            enabled: panel.canWrite && backend.keys.selectedCount === 1 && shallow.value < deep.value
                            onClicked: {
                                const list = []
                                for (let i = 0; i < 4; ++i) {
                                    const r = rows.itemAt(i)
                                    list.push({ key: r.key, ds: r.ds, de: r.de, us: r.us, ue: r.ue })
                                }
                                backend.createDks(shallow.value / 10, deep.value / 10, list)
                            }
                        }
                    }
                }
            }
        }
    }
}
