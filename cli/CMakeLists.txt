# SPDX-License-Identifier: MIT
add_executable(mchosectl src/main.cpp src/json.cpp)
target_link_libraries(mchosectl PRIVATE mchose::core mchose::hid)
target_compile_definitions(mchosectl PRIVATE MCHOSE_VERSION="${PROJECT_VERSION}")
install(TARGETS mchosectl RUNTIME DESTINATION bin)
