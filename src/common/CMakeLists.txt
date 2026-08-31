add_library(openhbx_common
  openhbx_common_skeleton.cpp
)
target_include_directories(openhbx_common PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_common PRIVATE openhbx_warnings)
set_property(TARGET openhbx_common PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::common ALIAS openhbx_common)
