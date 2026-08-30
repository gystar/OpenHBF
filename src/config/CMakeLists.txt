add_library(openhbx_config STATIC
  config_skeleton.cpp
  config_types.cpp
  hbf_config_schema.cpp
  hbf_product_profile.cpp
  resolved_hbf_config.cpp
  capability.cpp
  component_registry.cpp
  hbf_product_composer.cpp
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/build_contract.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/config_types.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/hbf_config_schema.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/hbf_product_profile.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/resolved_hbf_config.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/capability.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/component_registry.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/config/hbf_product_composer.h
)
target_include_directories(openhbx_config
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(openhbx_config PRIVATE openhbx_warnings)
set_property(TARGET openhbx_config PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::config ALIAS openhbx_config)
