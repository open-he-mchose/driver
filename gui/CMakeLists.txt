# SPDX-License-Identifier: GPL-3.0-only
# Qt 6 / Qt Quick GUI. GPL-3.0 (Qt open-source edition, linked dynamically); core/hid stay MIT.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
  qt_policy(SET QTP0004 NEW)
endif()

qt_add_executable(mchose-gui
  src/main.cpp
  src/backend.cpp
  src/backend.hpp
  src/keymodel.cpp
  src/keymodel.hpp
  src/macromodel.cpp
  src/macromodel.hpp
  src/worker.cpp
  src/worker.hpp
)

qt_add_qml_module(mchose-gui
  URI MchoseGui
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/KeyboardView.qml
    qml/TriggerPanel.qml
    qml/DepthGraph.qml
    qml/KeymapPanel.qml
    qml/MacroPanel.qml
    qml/AdvancedPanel.qml
    qml/ScriptPanel.qml
    qml/GamepadPanel.qml
    qml/LightingPanel.qml
    qml/DevicePanel.qml
    qml/Tray.qml
)

file(GLOB MCHOSE_GUI_EXAMPLES_ABS ${PROJECT_SOURCE_DIR}/script/examples/*.lua)
qt_add_resources(mchose-gui "lua_examples"
  PREFIX "/examples"
  BASE ${PROJECT_SOURCE_DIR}/script/examples
  FILES ${MCHOSE_GUI_EXAMPLES_ABS})
target_link_libraries(mchose-gui PRIVATE mchose::core mchose::hid mchose::script mchose::gamepad
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Qt6 REQUIRED COMPONENTS DBus)
  target_link_libraries(mchose-gui PRIVATE Qt6::DBus)   # is there a tray (StatusNotifierWatcher)?
endif()
if(WIN32)
  target_link_libraries(mchose-gui PRIVATE user32)   # AllowSetForegroundWindow (single instance)
endif()
target_compile_definitions(mchose-gui PRIVATE MCHOSE_VERSION="${PROJECT_VERSION}")

# Translations (Qt Linguist): gui/i18n/*.ts compiled into the binary; the language follows the system
# (MCHOSE_LANG=en overrides). Update the .ts files with the update_translations target (lupdate).
find_package(Qt6 6.5 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
  qt_add_translations(mchose-gui
    TS_FILES i18n/mchose-gui_ru.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete)
elseif(MCHOSE_REQUIRE_TRANSLATIONS)
  message(FATAL_ERROR "Qt LinguistTools not found (needed for the translations)")
else()
  message(STATUS "Qt LinguistTools not found: mchose-gui is built without translations")
endif()
set_target_properties(mchose-gui PROPERTIES WIN32_EXECUTABLE ON)
install(TARGETS mchose-gui RUNTIME DESTINATION bin)
