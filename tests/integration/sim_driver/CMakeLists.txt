set(SIM_DRIVER_CASE "${CMAKE_CURRENT_BINARY_DIR}/sim_driver_mixed_minimal.stcf.nc")

add_test(
    NAME generate_sim_driver_run_loop_case
    COMMAND ${CMAKE_COMMAND}
        -DPREPROC=$<TARGET_FILE:scau_preproc>
        -DCASE_PATH=${SIM_DRIVER_CASE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_case.cmake
)
set_tests_properties(generate_sim_driver_run_loop_case PROPERTIES
    FIXTURES_SETUP sim_driver_run_loop_case
)

add_executable(test_sim_driver_run_loop test_sim_driver_run_loop.cpp)
target_link_libraries(test_sim_driver_run_loop
    PRIVATE
        scau::sim_driver
        scau::warnings
        GTest::gtest_main
)
add_dependencies(test_sim_driver_run_loop scau_preproc)
add_test(NAME test_sim_driver_run_loop COMMAND test_sim_driver_run_loop)
set_tests_properties(test_sim_driver_run_loop PROPERTIES
    FIXTURES_REQUIRED sim_driver_run_loop_case
    ENVIRONMENT "SCAU_SIM_RUN_LOOP_CASE=${SIM_DRIVER_CASE}"
)

add_executable(test_sim_driver_checkpoint_rollback test_sim_driver_checkpoint_rollback.cpp)
target_link_libraries(test_sim_driver_checkpoint_rollback
    PRIVATE
        scau::sim_driver
        scau::warnings
        GTest::gtest_main
)
add_dependencies(test_sim_driver_checkpoint_rollback scau_preproc)
add_test(NAME test_sim_driver_checkpoint_rollback COMMAND test_sim_driver_checkpoint_rollback)
set_tests_properties(test_sim_driver_checkpoint_rollback PROPERTIES
    FIXTURES_REQUIRED sim_driver_run_loop_case
    ENVIRONMENT "SCAU_SIM_RUN_LOOP_CASE=${SIM_DRIVER_CASE}"
)

add_executable(test_sim_driver_whole_system_mass_audit test_sim_driver_whole_system_mass_audit.cpp)
target_link_libraries(test_sim_driver_whole_system_mass_audit
    PRIVATE
        scau::sim_driver
        scau::warnings
        GTest::gtest_main
)
add_dependencies(test_sim_driver_whole_system_mass_audit scau_preproc)
add_test(NAME test_sim_driver_whole_system_mass_audit COMMAND test_sim_driver_whole_system_mass_audit)
set_tests_properties(test_sim_driver_whole_system_mass_audit PROPERTIES
    FIXTURES_REQUIRED sim_driver_run_loop_case
    ENVIRONMENT "SCAU_SIM_RUN_LOOP_CASE=${SIM_DRIVER_CASE}"
)
