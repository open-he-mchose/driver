# SPDX-License-Identifier: MIT
add_executable(mchose_tests main.cpp test_core.cpp)
target_link_libraries(mchose_tests PRIVATE mchose::core)
add_test(NAME core COMMAND mchose_tests)

if(TARGET mchose::script)
  add_executable(mchose_script_tests main.cpp test_script.cpp)
  target_link_libraries(mchose_script_tests PRIVATE mchose::script)
  target_compile_definitions(mchose_script_tests PRIVATE MCHOSE_SCRIPT_EXAMPLES="${PROJECT_SOURCE_DIR}/script/examples")
  add_test(NAME script COMMAND mchose_script_tests)
endif()
