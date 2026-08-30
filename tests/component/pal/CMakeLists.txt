# S5 owns PAL component tests.
add_executable(openhbx_pal_component_test test_flash_pal.cpp)
target_link_libraries(openhbx_pal_component_test PRIVATE
  OpenHBX::pal OpenHBX::interconnect OpenHBX::system openhbf_warnings)
add_test(NAME openhbx_pal_component_test COMMAND openhbx_pal_component_test)
set_tests_properties(openhbx_pal_component_test PROPERTIES
  LABELS "openhbx_s5;openhbx_pal;openhbx_e2")
