add_library(openhbf_ftl geometry_mapper.cpp block_sequence.cpp zone_map.cpp
  channel_ftl.cpp)
target_include_directories(openhbf_ftl PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
target_link_libraries(openhbf_ftl PUBLIC openhbf_common openhbf_media)
target_link_libraries(openhbf_ftl PRIVATE openhbf_warnings)
add_library(OpenHBF::ftl ALIAS openhbf_ftl)
