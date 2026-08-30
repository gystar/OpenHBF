add_executable(test_controller
  test_controller.cpp
)
target_link_libraries(test_controller PRIVATE OpenHBF::openhbf)
target_compile_features(test_controller PRIVATE cxx_std_17)
add_test(NAME controller COMMAND test_controller)
