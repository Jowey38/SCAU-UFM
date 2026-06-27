add_executable(test_dpm_drag_decay test_dpm_drag_decay.cpp)
target_link_libraries(test_dpm_drag_decay
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dpm_drag_decay COMMAND test_dpm_drag_decay)
set_tests_properties(test_dpm_drag_decay PROPERTIES LABELS golden)
