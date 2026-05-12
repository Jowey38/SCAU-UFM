add_executable(test_surface_state test_surface_state.cpp)
target_link_libraries(test_surface_state
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface_state COMMAND test_surface_state)

add_executable(test_surface_step test_step.cpp)
target_link_libraries(test_surface_step
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface_step COMMAND test_surface_step)
