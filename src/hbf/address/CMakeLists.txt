add_library(openhbx_hbf_address STATIC
  hbf_geometry.cpp
  channel_topology.cpp
  hbf_address_mapper.cpp
  block_sequence.cpp
  zone_map.cpp
)
target_include_directories(openhbx_hbf_address PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_hbf_address PUBLIC openhbx_common openhbx_config
  PRIVATE openhbx_warnings)
set_property(TARGET openhbx_hbf_address PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::hbf_address ALIAS openhbx_hbf_address)
