add_executable(test_deficit_writeoff_replay test_deficit_writeoff_replay.cpp)
target_link_libraries(test_deficit_writeoff_replay
    PRIVATE
        scau::coupling_core
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_deficit_writeoff_replay COMMAND test_deficit_writeoff_replay)
set_tests_properties(test_deficit_writeoff_replay PROPERTIES LABELS golden)
