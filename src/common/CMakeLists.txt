add_library(openhbf_common
  config.cpp
  event.cpp
  event_queue.cpp
)

target_include_directories(openhbf_common
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openhbf_common PRIVATE openhbf_warnings)

add_library(OpenHBF::common ALIAS openhbf_common)

add_library(openhbx_common
  openhbx_common_skeleton.cpp
)
target_include_directories(openhbx_common PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(openhbx_common PRIVATE openhbx_warnings)
set_property(TARGET openhbx_common PROPERTY OPENHBX_ROLE PRODUCTION_COMPONENT)
add_library(OpenHBX::common ALIAS openhbx_common)
