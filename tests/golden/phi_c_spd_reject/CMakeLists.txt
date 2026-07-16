add_executable(test_phi_c_spd_reject test_phi_c_spd_reject.cpp)
target_link_libraries(test_phi_c_spd_reject
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_phi_c_spd_reject COMMAND test_phi_c_spd_reject)
set_tests_properties(test_phi_c_spd_reject PROPERTIES LABELS golden)
