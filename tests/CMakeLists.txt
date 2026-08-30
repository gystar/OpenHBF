add_subdirectory(unit/media)
add_subdirectory(build-test/controller)
add_subdirectory(build-contract)
add_subdirectory(unit/system)
add_subdirectory(unit/config)
add_subdirectory(component/config)
add_subdirectory(unit/media/nand)
add_subdirectory(unit/ras)
add_subdirectory(component/media)
add_subdirectory(unit/hbf/address)
add_subdirectory(component/hbf/address)
add_subdirectory(unit/interconnect)
add_subdirectory(component/pal)
add_subdirectory(unit/hbf/controller)
add_subdirectory(component/hbf/controller)
add_subdirectory(unit/hbf/host)
add_subdirectory(component/hbf/host)
add_subdirectory(system)
add_subdirectory(conformance)
add_subdirectory(performance)
# Legacy FTL/media experiment is intentionally not built; the new FTL
# contracts are tested under tests/build-test/ftl.

function(openhbf_add_integration_core_test target source)
  add_executable(${target} "${source}")
  target_link_libraries(${target} PRIVATE openhbf_integration openhbf_warnings)
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES LABELS openhbf_integration)
endfunction()

openhbf_add_integration_core_test(openhbf_integration_core_test
  integration/test_integration_core.cpp)
openhbf_add_integration_core_test(openhbf_system_core_test
  integration/test_open_hbf_system_core.cpp)
