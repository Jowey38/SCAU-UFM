add_executable(test_backwater_reverse_debit
    test_backwater_reverse_debit.cpp
)
target_link_libraries(test_backwater_reverse_debit
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_backwater_reverse_debit COMMAND test_backwater_reverse_debit)
set_tests_properties(test_backwater_reverse_debit PROPERTIES LABELS golden)
