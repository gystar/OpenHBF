add_executable(openhbx_s8_system_test test_open_hbx_system.cpp)
target_link_libraries(openhbx_s8_system_test PRIVATE openhbx openhbx_warnings)
add_test(NAME openhbx_s8_system_test COMMAND openhbx_s8_system_test)
set_tests_properties(openhbx_s8_system_test PROPERTIES
  LABELS "openhbx_s8;openhbx_system;E3"
  TIMEOUT 60)
