# SPDX-License-Identifier: MIT
# Analog keys as a virtual gamepad: key depth -> sticks / triggers / buttons.
#   Linux:   uinput (needs write access to /dev/uinput, see packaging/linux/72-mchose-uinput.rules)
#   Windows: ViGEmBus (the virtual-controller bus driver must be installed); client library ViGEmClient (MIT),
#            downloaded at configure time and linked statically.
# Presets (presets/*.txt) are compiled in.
file(GLOB MCHOSE_GAMEPAD_PRESETS ${CMAKE_CURRENT_SOURCE_DIR}/presets/*.txt)
set(MCHOSE_PRESET_ENTRIES "")
foreach(f ${MCHOSE_GAMEPAD_PRESETS})
  get_filename_component(n ${f} NAME_WE)
  file(READ ${f} body)
  string(APPEND MCHOSE_PRESET_ENTRIES "    {\"${n}\", R\"MCHOSE(${body})MCHOSE\"},\n")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MCHOSE_GAMEPAD_PRESETS})
configure_file(src/presets.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/presets.cpp @ONLY)

add_library(mchose_gamepad src/mapping.cpp src/virtual_pad.cpp ${CMAKE_CURRENT_BINARY_DIR}/presets.cpp)
add_library(mchose::gamepad ALIAS mchose_gamepad)
target_include_directories(mchose_gamepad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(mchose_gamepad PUBLIC cxx_std_20)

if(WIN32)
  include(FetchContent)
  FetchContent_Declare(vigemclient
    URL https://codeload.github.com/nefarius/ViGEmClient/tar.gz/9e842ba1c3a6efbb90d9b7e9346a55b1a3d10494
    URL_HASH SHA256=9a3d7d0c75a313c9703e0807ec89499e64a9c960f5b6f000709a1d629ea624d9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR do-not-add)   # only download; the library is built below
  FetchContent_MakeAvailable(vigemclient)
  # Build the client ourselves (v1.21.222.0): one source file, static, no resource file.
  add_library(mchose_vigemclient STATIC ${vigemclient_SOURCE_DIR}/src/ViGEmClient.cpp)
  target_include_directories(mchose_vigemclient PUBLIC ${vigemclient_SOURCE_DIR}/include)
  target_compile_definitions(mchose_vigemclient PRIVATE _CRT_SECURE_NO_WARNINGS)
  if(MSVC)
    target_compile_options(mchose_vigemclient PRIVATE /W0)
  endif()
  target_link_libraries(mchose_vigemclient PUBLIC setupapi)
  target_link_libraries(mchose_gamepad PRIVATE mchose_vigemclient)
  target_compile_definitions(mchose_gamepad PRIVATE MCHOSE_HAVE_VIGEM)
endif()
