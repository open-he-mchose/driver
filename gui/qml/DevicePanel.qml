// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Pane {
    ColumnLayout {
        width: Math.min(parent.width, 720)
        spacing: 10

        Label {
            text: backend.connected
                  ? qsTr("%1, firmware %2. Active profile %3; Fn+Enter rotation: %4. Report rate %5 Hz.")
                        .arg(backend.modelName).arg(backend.firmware).arg(backend.activeProfile)
                        .arg(backend.rotation.join(" ")).arg(backend.reportRateHz)
                  : qsTr("No keyboard connected. Close the official web driver and press Connect.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Switch {
                text: qsTr("Live key depth")
                checked: backend.streaming
                enabled: backend.connected && !backend.busy
                onToggled: backend.setStreaming(checked)
            }
            Label {
                opacity: 0.7
                text: qsTr("(press keys and watch the keyboard above; the stream pauses ~0.6 s after any other command)")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            spacing: 10
            Button {
                text: qsTr("Save backup…")
                enabled: backend.connected && !backend.busy
                onClicked: saveDialog.open()
            }
            Button {
                text: qsTr("Restore backup…")
                enabled: backend.connected && !backend.busy
                onClicked: openDialog.open()
            }
        }
        Label {
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            text: qsTr("A full backup is saved automatically before the first change of every session. Backups are compatible with mchosectl.")
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
