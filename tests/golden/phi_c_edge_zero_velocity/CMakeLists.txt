add_executable(test_phi_c_edge_zero_velocity test_phi_c_edge_zero_velocity.cpp)
target_link_libraries(test_phi_c_edge_zero_velocity
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_phi_c_edge_zero_velocity COMMAND test_phi_c_edge_zero_velocity)
set_tests_properties(test_phi_c_edge_zero_velocity PROPERTIES LABELS golden)
