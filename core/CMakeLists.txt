# SPDX-License-Identifier: MIT
# Protocol core: plain C++20, no dependencies (no Qt, no hidapi).
add_library(mchose_core
  src/packet.cpp
  src/session.cpp
  src/structs.cpp
  src/keycode.cpp
  src/macro.cpp
  src/macro_text.cpp
  src/model.cpp
  src/keyboard.cpp
  src/keyboard_adv.cpp
  src/advanced.cpp
)
add_library(mchose::core ALIAS mchose_core)
target_include_directories(mchose_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_features(mchose_core PUBLIC cxx_std_20)
