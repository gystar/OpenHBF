add_library(openhbx_hbf_controller STATIC
  controller_skeleton.cpp
  dlu_accumulator.cpp
  flash_scheduler.cpp
  bank_cache_manager.cpp
  ecc_pipeline.cpp
  control_plane.cpp
  base_die_flash_controller.cpp
  ${PROJECT_SOURCE_DIR}/include/openhbx/hbf/controller/build_contract.h)
target_include_directories(openhbx_hbf_controller PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_hbf_controller PUBLIC
  openhbx_common openhbx_system openhbx_hbf_address openhbx_pal)
target_link_libraries(openhbx_hbf_controller PRIVATE openhbf_warnings)
set_property(TARGET openhbx_hbf_controller PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::hbf_controller ALIAS openhbx_hbf_controller)
