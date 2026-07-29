add_executable(test_cvc_spatial_phi_t_dynamic test_cvc_spatial_phi_t_dynamic.cpp)
target_link_libraries(test_cvc_spatial_phi_t_dynamic
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_cvc_spatial_phi_t_dynamic COMMAND test_cvc_spatial_phi_t_dynamic)
set_tests_properties(test_cvc_spatial_phi_t_dynamic PROPERTIES LABELS golden)
