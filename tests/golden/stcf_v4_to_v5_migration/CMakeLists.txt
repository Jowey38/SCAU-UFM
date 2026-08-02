add_executable(test_stcf_v4_to_v5_migration
    test_stcf_v4_to_v5_migration.cpp
)
target_link_libraries(test_stcf_v4_to_v5_migration
    PRIVATE
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_stcf_v4_to_v5_migration COMMAND test_stcf_v4_to_v5_migration)
set_tests_properties(test_stcf_v4_to_v5_migration PROPERTIES LABELS golden)
