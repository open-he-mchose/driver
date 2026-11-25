// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    // Shown by main.cpp (not with --hidden). Closing hides the window; the program keeps running when
    // backend.keepInBackground is on (a second start shows it again), otherwise it quits.
    width: backend.uiValue("windowWidth", 1180)
    height: backend.uiValue("windowHeight", 820)
    minimumWidth: 900
    minimumHeight: 720
    title: "MCHOSE configurator " + backend.version + (backend.connected ? " — " + backend.modelName : "")

    property string lastMessage: ""
    property int startTab: 0
    property bool lastIsError: false

    Connections {
        target: backend
        function onMessage(text, error) { win.lastMessage = text; win.lastIsError = error }
    }

    function saveSize() {
        if (win.visibility === Window.Windowed) {
            backend.setUiValue("windowWidth", win.width)
            backend.setUiValue("windowHeight", win.height)
        }
    }
    onClosing: {
        saveSize()
        if (backend.calibrating)
            backend.finishCalibration()   // key presses elsewhere must not go into a running session
        if (!backend.keepInBackground) {
            Qt.quit()
        } else if (tray.item && tray.item.available && !backend.uiValue("trayHintShown", false)) {
            tray.item.showMessage(qsTr("MCHOSE configurator"),
                                  qsTr("Still running in the background. Quit from the tray icon menu or on the Device tab."))
            backend.setUiValue("trayHintShown", true)
        }
    }
    Connections {
        target: Qt.application
        function onAboutToQuit() { if (win.visible) win.saveSize() }
    }
    Shortcut { sequences: [StandardKey.Quit]; context: Qt.ApplicationShortcut; enabled: !backend.calibrating; onActivated: Qt.quit() }
    Loader {
        id: tray
        active: trayAvailable
        source: "Tray.qml"
        onLoaded: item.window = win
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 14
            Label {
                text: backend.connected ? backend.modelName : qsTr("No keyboard")
                font.bold: true
                font.pixelSize: 16
            }
            Label {
                text: backend.connected ? qsTr("firmware %1").arg(backend.firmware) : ""
                opacity: 0.7
            }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("Profile"); visible: backend.connected }
            Repeater {
                model: backend.connected ? 4 : 0
                delegate: Button {
                    required property int index
                    text: index + (backend.rotation.indexOf(index) >= 0 ? "" : "*")
                    enabled: !backend.busy && !backend.calibrating
                    flat: true
                    implicitWidth: 44
                    font.bold: backend.activeProfile === index
                    background: Rectangle {
                        radius: 4
                        color: backend.activeProfile === index ? palette.highlight : "transparent"
                        border.color: palette.mid
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: backend.rotation.indexOf(index) >= 0
                                  ? qsTr("Profile %1").arg(index)
                                  : qsTr("Profile %1 (* outside the Fn+Enter rotation)").arg(index)
                    onClicked: backend.switchProfile(index)
                }
            }
            Label {
                visible: backend.connected
                text: backend.reportRateHz ? qsTr("%1 Hz").arg(backend.reportRateHz) : ""
                opacity: 0.7
            }
            BusyIndicator { running: backend.busy; implicitWidth: 32; implicitHeight: 32 }
            Button {
                text: backend.connected ? qsTr("Reload") : qsTr("Connect")
                enabled: !backend.busy && !backend.calibrating
                onClicked: backend.connected ? backend.refresh() : backend.connectDevice()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        KeyboardView {
            id: kbd
            // Fixed size: the keyboard never changes size when the window or the tab changes.
            fixedUnit: 60
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: implicitWidth
            Layout.minimumWidth: implicitWidth
            Layout.maximumWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            Layout.minimumHeight: implicitHeight
            Layout.maximumHeight: implicitHeight
            overlay: backend.calibrating ? "calibration" : tabs.currentIndex === 0 ? "actuation" : tabs.currentIndex <= 3 ? "mapping"
                   : tabs.currentIndex === 4 ? "color" : tabs.currentIndex === 5 ? "none"
                   : tabs.currentIndex === 6 ? "gamepad" : "depth"
            enabled: backend.connected
            opacity: backend.connected ? 1 : 0.4
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TabBar {
                id: tabs
                Layout.preferredWidth: 720
                Layout.minimumWidth: 400
                currentIndex: win.startTab
                enabled: !backend.calibrating
                TabButton { text: qsTr("Actuation") }
                TabButton { text: qsTr("Keymap") }
                TabButton { text: qsTr("Macros") }
                TabButton { text: qsTr("Advanced") }
                TabButton { text: qsTr("Lighting") }
                TabButton { text: qsTr("Effects") }
                TabButton { text: qsTr("Gamepad") }
                TabButton { text: qsTr("Device") }
            }
            Label {
                text: backend.keys.selectedCount
                      ? qsTr("%n key(s) selected", "", backend.keys.selectedCount)
                      : qsTr("Click, Shift+click or drag to select keys")
                opacity: 0.8
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                Layout.fillWidth: true
                Layout.preferredWidth: 100
            }
            Button { text: qsTr("All"); flat: true; onClicked: backend.keys.selectAll() }
            Button { text: qsTr("None"); flat: true; onClicked: backend.keys.clearSelection() }
        }

        CalibrationPanel {
            visible: backend.calibrating
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        StackLayout {
            visible: !backend.calibrating
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            TriggerPanel {}
            KeymapPanel {}
            MacroPanel {}
            AdvancedPanel {}
            LightingPanel {}
            ScriptPanel {}
            GamepadPanel {}
            DevicePanel {}
        }
    }

    footer: Pane {
        padding: 6
        Label {
            width: parent.width
            text: win.lastMessage
            color: palette.windowText
            font.bold: win.lastIsError
            elide: Text.ElideRight
        }
    }
}
