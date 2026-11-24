# SPDX-License-Identifier: MIT
# HID transport through hidapi. Uses an installed hidapi (CMake package or pkg-config); otherwise downloads
# and builds it (MCHOSE_FETCH_HIDAPI).

set(MCHOSE_HIDAPI_TARGET "")

find_package(hidapi 0.10 QUIET CONFIG)
foreach(t hidapi::hidraw hidapi::hidapi hidapi::winapi hidapi::darwin)
  if(TARGET ${t} AND NOT MCHOSE_HIDAPI_TARGET)
    set(MCHOSE_HIDAPI_TARGET ${t})
  endif()
endforeach()

if(NOT MCHOSE_HIDAPI_TARGET)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(HIDAPI QUIET IMPORTED_TARGET hidapi-hidraw)
    if(NOT HIDAPI_FOUND)
      pkg_check_modules(HIDAPI QUIET IMPORTED_TARGET hidapi)
    endif()
    if(HIDAPI_FOUND)
      set(MCHOSE_HIDAPI_TARGET PkgConfig::HIDAPI)
    endif()
  endif()
endif()

if(NOT MCHOSE_HIDAPI_TARGET AND MCHOSE_FETCH_HIDAPI)
  message(STATUS "hidapi not found: downloading hidapi 0.14.0")
  include(FetchContent)
  set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
  set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG hidapi-0.14.0)
  FetchContent_MakeAvailable(hidapi)
  foreach(t hidapi::hidraw hidapi::hidapi hidapi::winapi hidapi::darwin)
    if(TARGET ${t} AND NOT MCHOSE_HIDAPI_TARGET)
      set(MCHOSE_HIDAPI_TARGET ${t})
    endif()
  endforeach()
endif()

if(NOT MCHOSE_HIDAPI_TARGET)
  message(FATAL_ERROR "hidapi not found. Install it (e.g. libhidapi-dev / hidapi-devel) or enable MCHOSE_FETCH_HIDAPI.")
endif()
message(STATUS "hidapi: ${MCHOSE_HIDAPI_TARGET}")

add_library(mchose_hid src/hidapi_transport.cpp)
add_library(mchose::hid ALIAS mchose_hid)
target_include_directories(mchose_hid PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(mchose_hid PUBLIC mchose::core PRIVATE ${MCHOSE_HIDAPI_TARGET})
