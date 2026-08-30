add_executable(openhbx_public_headers_contract public_headers_contract.cpp)
target_link_libraries(openhbx_public_headers_contract
  PRIVATE OpenHBX::openhbx openhbf_warnings)
add_test(NAME openhbx_public_headers_contract
  COMMAND openhbx_public_headers_contract)
set_tests_properties(openhbx_public_headers_contract PROPERTIES
  LABELS openhbx_build_contract)

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
foreach(legacy_target
    openhbf_common openhbf_media openhbf_ftl openhbf_controller
    openhbf_integration openhbf)
  if(legacy_target IN_LIST openhbx_components)
    message(FATAL_ERROR
      "Legacy target ${legacy_target} entered the OpenHBX production aggregate")
  endif()
endforeach()
