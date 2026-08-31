add_executable(openhbx_public_api_test test_public_api.cpp)
target_link_libraries(openhbx_public_api_test
  PRIVATE OpenHBX::openhbx openhbx_warnings)
add_test(NAME openhbx_public_api_test
  COMMAND openhbx_public_api_test)
set_tests_properties(openhbx_public_api_test PROPERTIES
  LABELS openhbx_public_api)

get_target_property(openhbx_components openhbx INTERFACE_LINK_LIBRARIES)
set(expected_openhbx_components
  openhbx_config
  openhbx_common
  openhbx_system
  openhbx_integration
  openhbx_hbf_host
  openhbx_hbf_address
  openhbx_hbf_controller
  openhbx_interconnect
  openhbx_pal
  openhbx_media_nand
  openhbx_ras
)
foreach(component IN LISTS expected_openhbx_components)
  if(NOT TARGET ${component})
    message(FATAL_ERROR "Missing OpenHBX production target ${component}")
  endif()
  if(NOT component IN_LIST openhbx_components)
    message(FATAL_ERROR
      "OpenHBX production aggregate does not link ${component}")
  endif()
  get_target_property(component_role ${component} OPENHBX_ROLE)
  if(NOT component_role STREQUAL "PRODUCTION_COMPONENT")
    message(FATAL_ERROR
      "Target ${component} is not marked as a production component")
  endif()
endforeach()
