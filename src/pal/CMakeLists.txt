add_library(openhbx_pal STATIC pal_skeleton.cpp
  flash_pal.cpp
  ${PROJECT_SOURCE_DIR}/include/openhbx/pal/build_contract.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/pal/flash_pal.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/pal/media_port.h)
target_include_directories(openhbx_pal PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_pal PUBLIC openhbx_common openhbx_system openhbx_interconnect)
target_link_libraries(openhbx_pal PRIVATE openhbx_warnings)
set_property(TARGET openhbx_pal PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::pal ALIAS openhbx_pal)
