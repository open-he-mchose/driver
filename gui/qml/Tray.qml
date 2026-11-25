// SPDX-License-Identifier: GPL-3.0-only
// Tray icon where the platform has one (Windows; Linux desktops with StatusNotifierItem, e.g. KDE, XFCE, GNOME with
// the AppIndicator extension). Loaded by Main.qml through a Loader, so a missing Qt.labs.platform only loses the tray.
import QtQuick
import Qt.labs.platform as Platform

Platform.SystemTrayIcon {
    id: tray
    property var window: null

    function showWindow() {
        if (!window)
            return
        if (window.visibility === Window.Minimized)
            window.showNormal()
        else
            window.show()
        window.raise()
        window.requestActivate()
    }

    visible: available
    icon.source: "image://app/icon"
    tooltip: backend.scriptRunning ? qsTr("MCHOSE configurator — Lua effect running")
           : backend.gamepadRunning ? qsTr("MCHOSE configurator — gamepad running")
           : qsTr("MCHOSE configurator")

    onActivated: (reason) => {
        if (reason === Platform.SystemTrayIcon.Trigger || reason === Platform.SystemTrayIcon.DoubleClick) {
            if (window && window.visible && window.active)
                window.hide()
            else
                showWindow()
        }
    }

    menu: Platform.Menu {
        Platform.MenuItem { text: qsTr("Show"); onTriggered: tray.showWindow() }
        Platform.MenuItem {
            text: qsTr("Stop the Lua effect")
            visible: backend.scriptRunning
            onTriggered: backend.stopScript()
        }
        Platform.MenuItem {
            text: qsTr("Stop the gamepad")
            visible: backend.gamepadRunning
            onTriggered: backend.stopGamepad()
        }
        Platform.MenuSeparator {}
        Platform.MenuItem { text: qsTr("Quit"); onTriggered: Qt.quit() }
    }
}
