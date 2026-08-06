set(WHOLE_SYSTEM_AUDIT_CASE
    "${CMAKE_CURRENT_BINARY_DIR}/whole_system_audit_mixed_minimal.stcf.nc")

add_test(
    NAME generate_whole_system_mass_audit_case
    COMMAND ${CMAKE_COMMAND}
        -DPREPROC=$<TARGET_FILE:scau_preproc>
        -DCASE_PATH=${WHOLE_SYSTEM_AUDIT_CASE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_case.cmake
)
set_tests_properties(generate_whole_system_mass_audit_case PROPERTIES
    FIXTURES_SETUP whole_system_mass_audit_case
    LABELS golden
)

add_executable(test_whole_system_mass_audit test_whole_system_mass_audit.cpp)
target_link_libraries(test_whole_system_mass_audit
    PRIVATE scau::sim_driver scau::warnings GTest::gtest_main)
add_dependencies(test_whole_system_mass_audit scau_preproc)
add_test(NAME test_whole_system_mass_audit COMMAND test_whole_system_mass_audit)
set_tests_properties(test_whole_system_mass_audit PROPERTIES
    FIXTURES_REQUIRED whole_system_mass_audit_case
    LABELS golden
    ENVIRONMENT "SCAU_SIM_RUN_LOOP_CASE=${WHOLE_SYSTEM_AUDIT_CASE}"
)
