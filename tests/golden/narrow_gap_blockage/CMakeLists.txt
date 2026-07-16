add_executable(test_narrow_gap_blockage test_narrow_gap_blockage.cpp)
target_link_libraries(test_narrow_gap_blockage
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_narrow_gap_blockage COMMAND test_narrow_gap_blockage)
set_tests_properties(test_narrow_gap_blockage PROPERTIES LABELS golden)
