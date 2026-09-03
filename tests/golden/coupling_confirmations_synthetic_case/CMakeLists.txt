add_executable(test_coupling_confirmations_synthetic_case
    test_coupling_confirmations_synthetic_case.cpp
)
target_compile_definitions(test_coupling_confirmations_synthetic_case
    PRIVATE
        SCAU_COUPLING_CONFIRMATIONS_CASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cases"
        SCAU_PREPROC_GIS_CASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../preproc_gis_synthetic_case/cases")
target_link_libraries(test_coupling_confirmations_synthetic_case
    PRIVATE
        scau::sim_driver
        scau::surface2d
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_confirmations_synthetic_case COMMAND test_coupling_confirmations_synthetic_case)
set_tests_properties(test_coupling_confirmations_synthetic_case PROPERTIES LABELS golden)
