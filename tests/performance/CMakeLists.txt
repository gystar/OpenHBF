add_executable(openhbx_flash_read_bandwidth_test
  test_flash_read_bandwidth.cpp)
target_link_libraries(openhbx_flash_read_bandwidth_test PRIVATE
  openhbx openhbx_warnings)
add_test(NAME openhbx_flash_read_bandwidth_test
  COMMAND openhbx_flash_read_bandwidth_test)
set_tests_properties(openhbx_flash_read_bandwidth_test PROPERTIES
  LABELS "openhbx_performance;openhbx_bandwidth;openhbx_e4_candidate;VER_PERF_001"
  TIMEOUT 120
  ENVIRONMENT
    "OPENHBX_BANDWIDTH_LOG=${CMAKE_SOURCE_DIR}/build/artifacts/performance/EXP-PERF-HOST-FLASH-READ/ctest/bandwidth.json")

add_executable(openhbx_hbf_max_read_bandwidth_test
  test_hbf_max_read_bandwidth.cpp)
target_link_libraries(openhbx_hbf_max_read_bandwidth_test PRIVATE
  openhbx openhbf_warnings)
add_test(NAME openhbx_hbf_max_read_bandwidth_test
  COMMAND openhbx_hbf_max_read_bandwidth_test)
set_tests_properties(openhbx_hbf_max_read_bandwidth_test PROPERTIES
  LABELS "openhbx_performance;openhbx_bandwidth;openhbx_e4_candidate;VER_PERF_002"
  TIMEOUT 180
  ENVIRONMENT
    "OPENHBX_MAX_BANDWIDTH_LOG=${CMAKE_SOURCE_DIR}/build/artifacts/performance/EXP-PERF-HBF-MAX-READ/ctest/bandwidth.json")
