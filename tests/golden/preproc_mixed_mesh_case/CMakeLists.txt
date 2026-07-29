set(PREPROC_CASE_A "${CMAKE_CURRENT_BINARY_DIR}/preproc_mixed_a.stcf.nc")
set(PREPROC_CASE_B "${CMAKE_CURRENT_BINARY_DIR}/preproc_mixed_b.stcf.nc")

add_test(
    NAME generate_preproc_mixed_mesh_cases
    COMMAND ${CMAKE_COMMAND}
        -DPREPROC=$<TARGET_FILE:scau_preproc>
        -DCASE_A=${PREPROC_CASE_A}
        -DCASE_B=${PREPROC_CASE_B}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_cases.cmake
)
set_tests_properties(generate_preproc_mixed_mesh_cases PROPERTIES
    FIXTURES_SETUP preproc_mixed_mesh_cases
    LABELS golden
)

add_executable(test_preproc_mixed_mesh_case test_preproc_mixed_mesh_case.cpp)
target_link_libraries(test_preproc_mixed_mesh_case
    PRIVATE
        scau::surface2d
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_dependencies(test_preproc_mixed_mesh_case scau_preproc)
add_test(NAME test_preproc_mixed_mesh_case COMMAND test_preproc_mixed_mesh_case)
set_tests_properties(test_preproc_mixed_mesh_case PROPERTIES
    FIXTURES_REQUIRED preproc_mixed_mesh_cases
    LABELS golden
    ENVIRONMENT
        "SCAU_PREPROC_CASE_A=${PREPROC_CASE_A};SCAU_PREPROC_CASE_B=${PREPROC_CASE_B}"
)
