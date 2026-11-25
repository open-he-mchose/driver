// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Pane {
    id: panel
    readonly property bool canWrite: backend.connected && !backend.busy

    function loadSettings() {
        winLock.checked = backend.winLock
        altTab.checked = backend.altTabLock
        altF4.checked = backend.altF4Lock
        menu.checked = backend.menuLock
        mac.checked = backend.macMode
        win.checked = !backend.macMode
        for (let p = 0; p < 4; ++p)
            rotationBoxes.itemAt(p).checked = backend.rotation.indexOf(p) >= 0
        const rates = [1000, 2000, 4000, 8000]
        rate.currentIndex = Math.max(0, rates.indexOf(backend.reportRateHz))
    }
    Component.onCompleted: loadSettings()
    Connections { target: backend; function onSnapshotChanged() { panel.loadSettings() } }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        GridLayout {
            width: Math.min(parent.width, 980)
            columns: 2
            columnSpacing: 30
            rowSpacing: 8

            // ---- left column
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: backend.connected
                          ? qsTr("%1, firmware %2. Active profile %3.").arg(backend.modelName).arg(backend.firmware).arg(backend.activeProfile)
                          : qsTr("No keyboard connected. Close the official web driver and press Connect.")
                }

                Label { text: qsTr("Profiles"); font.bold: true; Layout.topMargin: 6 }
                RowLayout {
                    Label { text: qsTr("Fn+Enter cycles through") }
                    Repeater {
                        id: rotationBoxes
                        model: 4
                        delegate: CheckBox { required property int index; text: index }
                    }
                    Button {
                        text: qsTr("Apply")
                        enabled: panel.canWrite
                        onClicked: {
                            const list = []
                            for (let p = 0; p < 4; ++p) if (rotationBoxes.itemAt(p).checked) list.push(p)
                            backend.setRotation(list)
                        }
                    }
                }
                RowLayout {
                    Label { text: qsTr("Copy profile") }
                    SpinBox { id: copySrc; from: 0; to: 3; value: backend.activeProfile; Layout.preferredWidth: 90 }
                    Label { text: qsTr("to") }
                    SpinBox { id: copyDst; from: 0; to: 3; value: (backend.activeProfile + 1) % 4; Layout.preferredWidth: 90 }
                    Button {
                        text: qsTr("Copy")
                        enabled: panel.canWrite && copySrc.value !== copyDst.value
                        onClicked: backend.copyProfile(copySrc.value, copyDst.value)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Settings, keymaps, colours and advanced keys; the destination's previous settings are overwritten")
                    }
                }

                Label { text: qsTr("Live key depth"); font.bold: true; Layout.topMargin: 6 }
                Switch {
                    text: qsTr("Show how deep every key is pressed")
                    checked: backend.streaming
                    enabled: panel.canWrite || backend.streaming
                    onToggled: backend.setStreaming(checked)
                }

                Label { text: qsTr("Backups"); font.bold: true; Layout.topMargin: 6 }
                RowLayout {
                    Button { text: qsTr("Save backup…"); enabled: panel.canWrite; onClicked: saveDialog.open() }
                    Button { text: qsTr("Restore backup…"); enabled: panel.canWrite; onClicked: openDialog.open() }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("A full backup is saved automatically before the first change of every session. Backups are compatible with mchosectl.")
                }
            }

            // ---- right column
            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                spacing: 6
                Label { text: qsTr("Report rate"); font.bold: true }
                RowLayout {
                    ComboBox { id: rate; model: ["1000 Hz", "2000 Hz", "4000 Hz", "8000 Hz"]; Layout.preferredWidth: 140 }
                    Button {
                        text: qsTr("Apply")
                        enabled: panel.canWrite && [1000, 2000, 4000, 8000][rate.currentIndex] !== backend.reportRateHz
                        onClicked: backend.setReportRate([1000, 2000, 4000, 8000][rate.currentIndex])
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("Applies to all profiles. The keyboard restarts; the app reconnects by itself.")
                }

                Label { text: qsTr("System keys (active profile)"); font.bold: true; Layout.topMargin: 6 }
                RowLayout {
                    RadioButton { id: win; text: qsTr("Windows") }
                    RadioButton { id: mac; text: qsTr("Mac") }
                }
                CheckBox { id: winLock; text: qsTr("Lock the Win keys (Windows mode only)") }
                CheckBox { id: altTab; text: qsTr("Lock Alt+Tab") }
                CheckBox { id: altF4; text: qsTr("Lock Alt+F4") }
                CheckBox { id: menu; text: qsTr("Lock the Menu key") }
                Button {
                    text: qsTr("Apply")
                    enabled: panel.canWrite
                    onClicked: backend.setSettings(winLock.checked, altTab.checked, altF4.checked, menu.checked, mac.checked)
                }

                Label { text: qsTr("Application"); font.bold: true; Layout.topMargin: 6 }
                CheckBox {
                    text: qsTr("Keep running in the background when the window is closed")
                    checked: backend.keepInBackground
                    onToggled: backend.keepInBackground = checked
                }
                CheckBox {
                    visible: backend.startAtLoginSupported
                    text: qsTr("Start at login (in the background)")
                    checked: backend.startAtLogin
                    onToggled: backend.startAtLogin = checked
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: qsTr("The running Lua effect or gamepad, its parameters and the editor texts are saved and started again when the keyboard connects. Starting the program again shows this window. While it runs, mchosectl cannot use the keyboard.")
                }
                Button { text: qsTr("Quit"); onClicked: Qt.quit() }
            }
        }
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["Backups (*.json)"]
        defaultSuffix: "json"
        onAccepted: backend.backup(selectedFile)
    }
    FileDialog {
        id: openDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["Backups (*.json)"]
        onAccepted: backend.restore(selectedFile)
    }
}
