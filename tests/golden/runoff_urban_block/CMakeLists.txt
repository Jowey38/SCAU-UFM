add_executable(test_golden_runoff_urban_block
    ${CMAKE_SOURCE_DIR}/tests/unit/surface2d/test_runoff_golden_urban_block.cpp
)
target_link_libraries(test_golden_runoff_urban_block
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_golden_runoff_urban_block COMMAND test_golden_runoff_urban_block)
set_tests_properties(test_golden_runoff_urban_block PROPERTIES LABELS "golden;runoff;m247")
