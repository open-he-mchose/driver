# SPDX-License-Identifier: MIT
# Lighting scripts in Lua. Lua 5.4 (MIT) is downloaded from lua.org and linked statically.
include(FetchContent)
FetchContent_Declare(lua
  URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
  URL_HASH SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(lua)

file(GLOB MCHOSE_LUA_SOURCES ${lua_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM MCHOSE_LUA_SOURCES ${lua_SOURCE_DIR}/src/lua.c ${lua_SOURCE_DIR}/src/luac.c)
add_library(mchose_lua STATIC ${MCHOSE_LUA_SOURCES})
target_include_directories(mchose_lua PUBLIC ${lua_SOURCE_DIR}/src)
set_target_properties(mchose_lua PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
if(MSVC)
  target_compile_options(mchose_lua PRIVATE /W0)
else()
  target_compile_options(mchose_lua PRIVATE -w)
  if(NOT WIN32)
    target_compile_definitions(mchose_lua PRIVATE LUA_USE_POSIX)
  endif()
endif()

add_library(mchose_script src/lighting_script.cpp src/key_input.cpp src/script_player.cpp)
add_library(mchose::script ALIAS mchose_script)
target_include_directories(mchose_script PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(mchose_script PUBLIC mchose::core PRIVATE mchose_lua)
