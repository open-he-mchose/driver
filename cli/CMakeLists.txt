# SPDX-License-Identifier: MIT
add_executable(mchosectl src/main.cpp src/json.cpp)
target_link_libraries(mchosectl PRIVATE mchose::core mchose::hid)
install(TARGETS mchosectl RUNTIME DESTINATION bin)
