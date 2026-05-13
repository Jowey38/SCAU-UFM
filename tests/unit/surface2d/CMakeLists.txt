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

add_executable(test_dpm_fields test_dpm_fields.cpp)
target_link_libraries(test_dpm_fields
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dpm_fields COMMAND test_dpm_fields)

add_executable(test_hydrostatic_reconstruction test_hydrostatic_reconstruction.cpp)
target_link_libraries(test_hydrostatic_reconstruction
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hydrostatic_reconstruction COMMAND test_hydrostatic_reconstruction)

add_executable(test_hllc_flux test_hllc_flux.cpp)
target_link_libraries(test_hllc_flux
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_flux COMMAND test_hllc_flux)

add_executable(test_phi_t_source test_phi_t_source.cpp)
target_link_libraries(test_phi_t_source
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_phi_t_source COMMAND test_phi_t_source)

add_executable(test_surface2d_golden_minimal test_surface2d_golden_minimal.cpp)
target_link_libraries(test_surface2d_golden_minimal
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_surface2d_golden_minimal COMMAND test_surface2d_golden_minimal)
