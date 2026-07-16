add_executable(test_hydrostatic_step test_hydrostatic_step.cpp)
target_link_libraries(test_hydrostatic_step
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hydrostatic_step COMMAND test_hydrostatic_step)
set_tests_properties(test_hydrostatic_step PROPERTIES LABELS golden)
