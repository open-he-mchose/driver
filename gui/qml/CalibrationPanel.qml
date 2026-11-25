// SPDX-License-Identifier: GPL-3.0-only
// Switch calibration session (0xA8 ... 0xA9). Shown instead of the tabs while it runs; the keyboard view shows each
// key's state (0/2, 1/2, ✓, ! = never calibrated). Key presses are swallowed so they do not reach other controls.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    id: panel

    function finish() {
        if (backend.keys.calibProgress > 0)
            confirmFinish.open()
        else
            backend.finishCalibration()
    }

    // Every key pressed during the session is for the calibration: keep it away from the window's controls.
    Item {
        id: keyEater
        focus: true
        Keys.onPressed: (event) => event.accepted = true
        Keys.onReleased: (event) => event.accepted = true
        Keys.onShortcutOverride: (event) => event.accepted = true
    }
    onVisibleChanged: if (visible) keyEater.forceActiveFocus()
    Component.onCompleted: keyEater.forceActiveFocus()

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: qsTr("Switch calibration"); font.bold: true; font.pixelSize: 16 }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Press each key you want to calibrate all the way down and hold it there for a moment, twice. "
                       + "The keys show 0/2, 1/2 and ✓ when done. Keys you do not press keep their calibration.")
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.8
            text: qsTr("Press firmly to the bottom: a key held part-way can be stored with a wrong bottom value. A key "
                       + "you press but do not finish keeps a default value until it is calibrated again. Key presses "
                       + "do not reach this window; system shortcuts (like the Win key) still work.")
        }

        RowLayout {
            spacing: 24
            Layout.topMargin: 6
            Label { text: qsTr("Done: %1").arg(backend.keys.calibDone); font.bold: true }
            Label { text: qsTr("Half-way: %1").arg(backend.keys.calibProgress); font.bold: backend.keys.calibProgress > 0 }
            Label { text: qsTr("Never calibrated: %1").arg(backend.keys.calibNeeded) }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: backend.keys.calibProgress > 0
            // calibProgress makes the binding follow every change (all counters share one notify signal)
            text: backend.keys.calibProgress >= 0 ? qsTr("Half-way: %1").arg(backend.keys.calibKeys("progress").join(" ")) : ""
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: backend.keys.calibNeeded > 0
            opacity: 0.8
            text: backend.keys.calibNeeded >= 0 ? qsTr("Never calibrated: %1").arg(backend.keys.calibKeys("needed").join(" ")) : ""
        }

        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Finish calibration")
                highlighted: true
                focusPolicy: Qt.NoFocus   // keeps the key eater focused
                onClicked: panel.finish()
            }
        }
    }

    Dialog {
        id: confirmFinish
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Keys not finished")
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: 420
            wrapMode: Text.WordWrap
            text: backend.keys.calibProgress < 0 ? "" : qsTr("%1 keep a default bottom value until they are calibrated again (press them twice more now, or "
                       + "run the calibration later). Finish anyway?").arg(backend.keys.calibKeys("progress").join(" "))
        }
        onAccepted: backend.finishCalibration()
        onClosed: keyEater.forceActiveFocus()
    }
}
