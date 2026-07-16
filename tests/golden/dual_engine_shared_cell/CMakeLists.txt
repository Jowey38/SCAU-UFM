add_executable(test_dual_engine_shared_cell test_dual_engine_shared_cell.cpp)
target_link_libraries(test_dual_engine_shared_cell
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dual_engine_shared_cell COMMAND test_dual_engine_shared_cell)
set_tests_properties(test_dual_engine_shared_cell PROPERTIES LABELS "candidate_non_gating")
