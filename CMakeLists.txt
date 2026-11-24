# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)
project(mchose-driver VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MCHOSE_BUILD_CLI "Build the hid transport and the mchosectl command-line tool" ON)
option(MCHOSE_BUILD_TESTS "Build the unit tests (core only, no keyboard needed)" ON)
option(MCHOSE_FETCH_HIDAPI "Download and build hidapi if it is not installed" ON)
option(MCHOSE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

if(MCHOSE_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)

if(MCHOSE_BUILD_CLI)
  add_subdirectory(hid)
  add_subdirectory(cli)
endif()

if(MCHOSE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
