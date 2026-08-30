add_library(openhbf_controller
  controller_types.cpp
  backend.cpp
  dlu_accumulator.cpp
  base_die_controller.cpp
  control_plane.cpp
)
target_include_directories(openhbf_controller PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries(openhbf_controller PUBLIC openhbf_common)
target_link_libraries(openhbf_controller PRIVATE openhbf_warnings)
add_library(OpenHBF::controller ALIAS openhbf_controller)
