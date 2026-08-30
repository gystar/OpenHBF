add_executable(openhbx_ras_unit_test test_ras.cpp)
target_link_libraries(openhbx_ras_unit_test PRIVATE openhbx_ras openhbx_warnings)
add_test(NAME openhbx_ras_unit_test COMMAND openhbx_ras_unit_test)
set_tests_properties(openhbx_ras_unit_test PROPERTIES LABELS "openhbx_s3;openhbx_ras;VER-MEDIA-005;VER-MEDIA-006")
