add_library(openhbx_system
  clock_domain.cpp
  completion_registry.cpp
  event_queue.cpp
  direct_driver.cpp
  open_hbx_system.cpp
  system_lifecycle.cpp
  system_skeleton.cpp
)
target_include_directories(openhbx_system PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_system PUBLIC
  openhbx_common
  openhbx_config
  openhbx_hbf_host
  openhbx_hbf_address
  openhbx_hbf_controller
  openhbx_interconnect
  openhbx_pal
  openhbx_media_nand
  openhbx_ras
  openhbx_integration
  PRIVATE openhbx_warnings)
set_property(TARGET openhbx_system PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::system ALIAS openhbx_system)
