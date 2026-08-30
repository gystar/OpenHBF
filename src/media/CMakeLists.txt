add_library(openhbf_media
  types.cpp
  topology.cpp
  state.cpp
  page_store.cpp
  eat.cpp
  timing.cpp
  command_engine.cpp
  reliability.cpp
  bbt.cpp
  die_environment.cpp
  media_stats.cpp
  persistence.cpp
  nand_media.cpp
)

target_include_directories(openhbf_media
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openhbf_media PUBLIC openhbf_common)
target_link_libraries(openhbf_media PRIVATE openhbf_warnings)

add_library(OpenHBF::media ALIAS openhbf_media)
