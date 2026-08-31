cmake_minimum_required(VERSION 3.16)

project(OpenHBX VERSION 0.1.0 LANGUAGES CXX)

option(OPENHBF_BUILD_TESTS "Build OpenHBF tests" OFF)
option(OPENHBF_WITH_RAMULATOR2 "Build the optional Ramulator2 adapter" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(openhbx_warnings INTERFACE)
target_compile_options(openhbx_warnings INTERFACE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Werror>
)

# Components are added explicitly as they are implemented. This keeps a
# successful configuration from silently omitting a misspelled source file.
add_subdirectory(src/common)
add_subdirectory(src/integration)

# OpenHBX production components.
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

if(OPENHBF_WITH_RAMULATOR2)
  message(STATUS
    "Building the OpenHBX Ramulator ABI adapter library. The stock Request ABI "
    "does not carry functional write payload bytes.")
endif()

if(OPENHBF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
