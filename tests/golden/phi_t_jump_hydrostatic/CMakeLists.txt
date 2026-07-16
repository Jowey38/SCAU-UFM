add_executable(test_phi_t_jump_hydrostatic test_phi_t_jump_hydrostatic.cpp)
target_link_libraries(test_phi_t_jump_hydrostatic
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_phi_t_jump_hydrostatic COMMAND test_phi_t_jump_hydrostatic)
set_tests_properties(test_phi_t_jump_hydrostatic PROPERTIES LABELS golden)
