add_executable(test_stcf_case_pipeline
    test_stcf_case_pipeline.cpp
)
target_link_libraries(test_stcf_case_pipeline
    PRIVATE
        scau::surface2d
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_stcf_case_pipeline COMMAND test_stcf_case_pipeline)
set_tests_properties(test_stcf_case_pipeline PROPERTIES LABELS golden)
