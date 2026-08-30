add_executable(openhbx_hbf_host_component_test test_host_protocol_component.cpp)
target_link_libraries(openhbx_hbf_host_component_test PRIVATE
  OpenHBX::hbf_host OpenHBX::hbf_controller OpenHBX::hbf_address
  OpenHBX::pal OpenHBX::interconnect OpenHBX::system openhbf_warnings)
add_test(NAME openhbx_hbf_host_component_test COMMAND openhbx_hbf_host_component_test)
set_tests_properties(openhbx_hbf_host_component_test PROPERTIES
  LABELS "openhbx_s7;openhbx_host;openhbx_e2")
