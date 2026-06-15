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

add_executable(test_cfl_rollback test_cfl_rollback.cpp)
target_link_libraries(test_cfl_rollback
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_cfl_rollback COMMAND test_cfl_rollback)

add_executable(test_dpm_edge_source_conservation test_dpm_edge_source_conservation.cpp)
target_link_libraries(test_dpm_edge_source_conservation
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dpm_edge_source_conservation COMMAND test_dpm_edge_source_conservation)

add_executable(test_conservative_update test_conservative_update.cpp)
target_link_libraries(test_conservative_update
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_conservative_update COMMAND test_conservative_update)

add_executable(test_wetting_drying test_wetting_drying.cpp)
target_link_libraries(test_wetting_drying
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_wetting_drying COMMAND test_wetting_drying)

add_executable(test_boundary_flux test_boundary_flux.cpp)
target_link_libraries(test_boundary_flux
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_boundary_flux COMMAND test_boundary_flux)

add_executable(test_boundary_conditions test_boundary_conditions.cpp)
target_link_libraries(test_boundary_conditions
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_boundary_conditions COMMAND test_boundary_conditions)

add_executable(test_momentum_transport test_momentum_transport.cpp)
target_link_libraries(test_momentum_transport
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_momentum_transport COMMAND test_momentum_transport)

add_executable(test_pressure_momentum test_pressure_momentum.cpp)
target_link_libraries(test_pressure_momentum
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_pressure_momentum COMMAND test_pressure_momentum)

add_executable(test_hllc_wave_momentum test_hllc_wave_momentum.cpp)
target_link_libraries(test_hllc_wave_momentum
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_wave_momentum COMMAND test_hllc_wave_momentum)

add_executable(test_hllc_wave_mass test_hllc_wave_mass.cpp)
target_link_libraries(test_hllc_wave_mass
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_hllc_wave_mass COMMAND test_hllc_wave_mass)

add_executable(test_step_rollback_rejection test_step_rollback_rejection.cpp)
target_link_libraries(test_step_rollback_rejection
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_step_rollback_rejection COMMAND test_step_rollback_rejection)

add_executable(test_runoff_fields test_runoff_fields.cpp)
target_link_libraries(test_runoff_fields
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_runoff_fields COMMAND test_runoff_fields)

add_executable(test_runoff_soil_params test_runoff_soil_params.cpp)
target_link_libraries(test_runoff_soil_params
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_runoff_soil_params COMMAND test_runoff_soil_params)

add_executable(test_runoff_green_ampt test_runoff_green_ampt.cpp)
target_link_libraries(test_runoff_green_ampt
    PRIVATE
        scau::surface2d
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_runoff_green_ampt COMMAND test_runoff_green_ampt)
