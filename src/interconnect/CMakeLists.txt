add_library(openhbx_interconnect STATIC
  interconnect_skeleton.cpp
  fabric.cpp
  fabric_profile.cpp
  tsv_repair.cpp
  ${PROJECT_SOURCE_DIR}/include/openhbx/interconnect/build_contract.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/interconnect/types.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/interconnect/fabric.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/interconnect/fabric_profile.h
  ${PROJECT_SOURCE_DIR}/include/openhbx/interconnect/tsv_repair.h)
target_include_directories(openhbx_interconnect PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_interconnect PUBLIC openhbx_common PRIVATE openhbf_warnings)
set_property(TARGET openhbx_interconnect PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::interconnect ALIAS openhbx_interconnect)
