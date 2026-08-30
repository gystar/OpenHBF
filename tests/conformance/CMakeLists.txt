add_executable(openhbx_s9_ocp_transaction_vectors
  test_ocp_transaction_vectors.cpp)
target_link_libraries(openhbx_s9_ocp_transaction_vectors PRIVATE
  openhbx openhbx_warnings)
target_compile_definitions(openhbx_s9_ocp_transaction_vectors PRIVATE
  OPENHBX_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME openhbx_s9_ocp_transaction_vectors
  COMMAND openhbx_s9_ocp_transaction_vectors)
set_tests_properties(openhbx_s9_ocp_transaction_vectors PROPERTIES
  LABELS "openhbx_s9;openhbx_conformance;E4_candidate"
  TIMEOUT 60)

add_executable(openhbx_s9_host_admission_oracle
  test_host_admission_oracle.cpp)
target_link_libraries(openhbx_s9_host_admission_oracle PRIVATE
  openhbx_hbf_host openhbx_hbf_address openhbx_warnings)
add_test(NAME openhbx_s9_host_admission_oracle
  COMMAND openhbx_s9_host_admission_oracle)
set_tests_properties(openhbx_s9_host_admission_oracle PROPERTIES
  LABELS "openhbx_s9;openhbx_conformance;E2"
  TIMEOUT 30)

add_test(NAME openhbx_s9_manifest_audit
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/audit_manifests.cmake)
set_tests_properties(openhbx_s9_manifest_audit PROPERTIES
  LABELS "openhbx_s9;openhbx_conformance;artifact_audit"
  TIMEOUT 30)
