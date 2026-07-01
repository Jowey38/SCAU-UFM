add_executable(test_swmm_single_pipe_surcharge test_swmm_single_pipe_surcharge.cpp)
target_link_libraries(test_swmm_single_pipe_surcharge
    PRIVATE
        scau::coupling_core
        scau::coupling_drainage
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_swmm_single_pipe_surcharge COMMAND test_swmm_single_pipe_surcharge)
set_tests_properties(test_swmm_single_pipe_surcharge PROPERTIES LABELS candidate_non_gating)
