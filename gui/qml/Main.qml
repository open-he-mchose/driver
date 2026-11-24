// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1180
    height: 820
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "MCHOSE configurator " + backend.version + (backend.connected ? " — " + backend.modelName : "")

    property string lastMessage: ""
    property int startTab: 0
    property bool lastIsError: false

    Connections {
        target: backend
        function onMessage(text, error) { win.lastMessage = text; win.lastIsError = error }
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
                    enabled: !backend.busy
                    flat: true
                    implicitWidth: 44
                    font.bold: backend.activeProfile === index
                    contentItem: Label {
                        text: parent.text
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: backend.activeProfile === index ? palette.highlightedText : palette.buttonText
                    }
                    background: Rectangle {
                        radius: 4
                        color: backend.activeProfile === index ? palette.highlight : "transparent"
                        border.color: backend.activeProfile === index ? palette.highlight : palette.mid
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
                enabled: !backend.busy
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
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(width / backend.keys.widthUnits * backend.keys.heightUnits + 8, win.height * 0.58)
            overlay: tabs.currentIndex === 0 ? "actuation" : tabs.currentIndex === 1 ? "mapping"
                   : tabs.currentIndex === 2 ? "color" : "depth"
            enabled: backend.connected
            opacity: backend.connected ? 1 : 0.4
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TabBar {
                id: tabs
                Layout.preferredWidth: 520
                currentIndex: win.startTab
                TabButton { text: qsTr("Actuation") }
                TabButton { text: qsTr("Keymap") }
                TabButton { text: qsTr("Lighting") }
                TabButton { text: qsTr("Device") }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: backend.keys.selectedCount
                      ? qsTr("%n key(s) selected", "", backend.keys.selectedCount)
                      : qsTr("Click, Shift+click or drag to select keys")
                opacity: 0.8
            }
            Button { text: qsTr("All"); flat: true; onClicked: backend.keys.selectAll() }
            Button { text: qsTr("None"); flat: true; onClicked: backend.keys.clearSelection() }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex
            TriggerPanel {}
            KeymapPanel {}
            LightingPanel {}
            DevicePanel {}
        }
    }

    footer: Pane {
        padding: 6
        Label {
            width: parent.width
            text: win.lastMessage
            color: win.lastIsError ? "#d32f2f" : palette.windowText
            elide: Text.ElideRight
        }
    }
}
