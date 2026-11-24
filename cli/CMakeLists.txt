# SPDX-License-Identifier: MIT
add_executable(mchosectl src/main.cpp)
target_link_libraries(mchosectl PRIVATE mchose::core mchose::hid)
target_compile_definitions(mchosectl PRIVATE MCHOSE_VERSION="${PROJECT_VERSION}")
if(TARGET mchose::script)
  target_link_libraries(mchosectl PRIVATE mchose::script)
  target_compile_definitions(mchosectl PRIVATE MCHOSE_HAVE_SCRIPT)
endif()
install(TARGETS mchosectl RUNTIME DESTINATION bin)
