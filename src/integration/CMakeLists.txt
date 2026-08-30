add_library(openhbf_integration
  host_request.cpp
  completion_registry.cpp
  open_hbf_config.cpp
  open_hbf_system.cpp
  direct_runner.cpp
)

target_include_directories(openhbf_integration
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries(openhbf_integration PUBLIC openhbf_common)
target_link_libraries(openhbf_integration PRIVATE openhbf_warnings)
add_library(OpenHBF::integration ALIAS openhbf_integration)

add_library(openhbx_integration
  integration_skeleton.cpp
  nand_media_port.cpp
  request_bridge.cpp
)
target_include_directories(openhbx_integration PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_integration PUBLIC
  openhbx_common openhbx_hbf_host openhbx_media_nand openhbx_pal
  PRIVATE openhbx_warnings)
set_property(TARGET openhbx_integration PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::integration ALIAS openhbx_integration)

if(OPENHBF_WITH_RAMULATOR2)
  add_library(openhbx_ramulator_adapter STATIC ramulator_memory_system.cpp)
  set_property(TARGET openhbx_ramulator_adapter PROPERTY CXX_STANDARD 20)
  target_compile_definitions(openhbx_ramulator_adapter
    PRIVATE OPENHBF_WITH_RAMULATOR2=1)
  target_include_directories(openhbx_ramulator_adapter
    PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_include_directories(openhbx_ramulator_adapter SYSTEM
    PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/ramulator2/src)
  target_link_libraries(openhbx_ramulator_adapter
    PRIVATE openhbx_system openhbx_integration openhbx_warnings)
  set_property(TARGET openhbx_ramulator_adapter
    PROPERTY OPENHBX_ROLE PRODUCTION_ADAPTER)
  add_library(OpenHBX::ramulator_adapter ALIAS openhbx_ramulator_adapter)
endif()
