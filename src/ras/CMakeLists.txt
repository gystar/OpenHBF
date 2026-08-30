add_library(openhbx_ras STATIC
  ras_skeleton.cpp
  bad_block_table.cpp
  reliability_model.cpp
)
target_include_directories(openhbx_ras PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_ras PUBLIC openhbx_common openhbx_config openhbx_warnings)
set_property(TARGET openhbx_ras PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::ras ALIAS openhbx_ras)
