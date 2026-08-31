# S5 owns interconnect unit tests.
add_executable(openhbx_interconnect_unit_test test_fabric.cpp)
target_link_libraries(openhbx_interconnect_unit_test PRIVATE
  OpenHBX::interconnect openhbx_warnings)
add_test(NAME openhbx_interconnect_unit_test COMMAND openhbx_interconnect_unit_test)
set_tests_properties(openhbx_interconnect_unit_test PROPERTIES
  LABELS "openhbx_s5;openhbx_interconnect;openhbx_e1")
