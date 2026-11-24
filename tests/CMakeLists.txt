# SPDX-License-Identifier: MIT
add_executable(mchose_tests main.cpp test_core.cpp)
target_link_libraries(mchose_tests PRIVATE mchose::core)
add_test(NAME core COMMAND mchose_tests)
