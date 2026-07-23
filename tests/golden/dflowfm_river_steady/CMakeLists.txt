add_executable(test_dflowfm_river_steady test_dflowfm_river_steady.cpp)
target_link_libraries(test_dflowfm_river_steady
    PRIVATE
        scau::coupling_driver
        scau::coupling_river
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dflowfm_river_steady COMMAND test_dflowfm_river_steady)
set_tests_properties(test_dflowfm_river_steady PROPERTIES LABELS "candidate_non_gating")
