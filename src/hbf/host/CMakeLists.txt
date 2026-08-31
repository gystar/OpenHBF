add_library(openhbx_hbf_host STATIC
  host_skeleton.cpp
  host_validator.cpp
  ordering_scoreboard.cpp
  packet_codec.cpp
  register_port.cpp
  controller_port.cpp
  host_protocol.cpp
  ${PROJECT_SOURCE_DIR}/include/openhbx/hbf/host/build_contract.h)
target_include_directories(openhbx_hbf_host PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_hbf_host PUBLIC
  openhbx_common openhbx_hbf_address openhbx_hbf_controller)
target_link_libraries(openhbx_hbf_host PRIVATE openhbx_warnings)
set_property(TARGET openhbx_hbf_host PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::hbf_host ALIAS openhbx_hbf_host)
