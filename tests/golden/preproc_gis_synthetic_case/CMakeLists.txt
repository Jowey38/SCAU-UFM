add_executable(test_preproc_gis_synthetic_case
    test_preproc_gis_synthetic_case.cpp
)
target_compile_definitions(test_preproc_gis_synthetic_case
    PRIVATE SCAU_PREPROC_GIS_CASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cases")
target_link_libraries(test_preproc_gis_synthetic_case
    PRIVATE
        scau::surface2d
        scau::stcf
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_preproc_gis_synthetic_case COMMAND test_preproc_gis_synthetic_case)
set_tests_properties(test_preproc_gis_synthetic_case PROPERTIES LABELS golden)
