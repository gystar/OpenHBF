cmake_minimum_required(VERSION 3.16)

project(OpenHBX VERSION 0.1.0 LANGUAGES CXX)

option(OPENHBF_BUILD_TESTS "Build OpenHBF tests" OFF)
option(OPENHBF_WITH_RAMULATOR2 "Build the optional Ramulator2 adapter" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(openhbf_warnings INTERFACE)
target_compile_options(openhbf_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)
add_library(openhbx_warnings ALIAS openhbf_warnings)

# Components are added explicitly as they are implemented. This keeps a
# successful configuration from silently omitting a misspelled source file.
add_subdirectory(src/common)
add_subdirectory(src/media)
add_subdirectory(src/ftl)
add_subdirectory(src/controller)
add_subdirectory(src/integration)
foreach(legacy_target
    openhbf_common openhbf_media openhbf_ftl openhbf_controller
    openhbf_integration)
  set_property(TARGET ${legacy_target} PROPERTY OPENHBX_ROLE LEGACY)
endforeach()

# OpenHBX production skeleton. These targets deliberately do not link the
# legacy OpenHBF implementation; later stages fill them in domain by domain.
add_subdirectory(src/config)
add_subdirectory(src/system)
add_subdirectory(src/hbf/host)
add_subdirectory(src/hbf/address)
add_subdirectory(src/hbf/controller)
add_subdirectory(src/interconnect)
add_subdirectory(src/pal)
add_subdirectory(src/media/nand)
add_subdirectory(src/ras)

add_library(openhbx INTERFACE)
target_link_libraries(openhbx INTERFACE
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
add_library(OpenHBX::openhbx ALIAS openhbx)
set_property(TARGET openhbx PROPERTY OPENHBX_ROLE PRODUCTION_AGGREGATE)

add_executable(openhbx_trace_runner apps/openhbx_trace_runner.cpp)
target_link_libraries(openhbx_trace_runner PRIVATE openhbx openhbx_warnings)

# Keep a stable public target while the remaining component libraries are
# implemented. Future host/controller/ftl/media/integration targets should be
# linked here after their explicit source lists and public contracts exist.
add_library(openhbf INTERFACE)
target_link_libraries(openhbf INTERFACE openhbf_common openhbf_media openhbf_ftl
  openhbf_controller openhbf_integration)
add_library(OpenHBF::openhbf ALIAS openhbf)
set_property(TARGET openhbf PROPERTY OPENHBX_ROLE LEGACY)

if(OPENHBF_WITH_RAMULATOR2)
  message(STATUS
    "Building the OpenHBX Ramulator ABI adapter library. The stock Request ABI "
    "does not carry functional write payload bytes.")
endif()

if(OPENHBF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
