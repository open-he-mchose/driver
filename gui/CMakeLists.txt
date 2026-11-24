# SPDX-License-Identifier: GPL-3.0-only
# Qt 6 / Qt Quick GUI. GPL-3.0 (Qt open-source edition, linked dynamically); core/hid stay MIT.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
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
    qml/LightingPanel.qml
    qml/DevicePanel.qml
)

file(GLOB MCHOSE_GUI_EXAMPLES_ABS ${PROJECT_SOURCE_DIR}/script/examples/*.lua)
qt_add_resources(mchose-gui "lua_examples"
  PREFIX "/examples"
  BASE ${PROJECT_SOURCE_DIR}/script/examples
  FILES ${MCHOSE_GUI_EXAMPLES_ABS})
target_link_libraries(mchose-gui PRIVATE mchose::core mchose::hid mchose::script
  Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_definitions(mchose-gui PRIVATE MCHOSE_VERSION="${PROJECT_VERSION}")
set_target_properties(mchose-gui PROPERTIES WIN32_EXECUTABLE ON)
install(TARGETS mchose-gui RUNTIME DESTINATION bin)
