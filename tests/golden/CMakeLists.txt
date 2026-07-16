include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(hydrostatic_step)
add_subdirectory(phi_t_jump_hydrostatic)
add_subdirectory(phi_c_edge_zero_velocity)
add_subdirectory(narrow_gap_blockage)
add_subdirectory(dpm_drag_decay)
add_subdirectory(phi_c_spd_reject)
add_subdirectory(swmm_single_pipe_surcharge)
add_subdirectory(snapshot_replay_mass_deficit)
add_subdirectory(runoff_urban_block)
add_subdirectory(roof_swmm_transfer)

add_test(
    NAME test_goldensuite_manifest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/suite_manifest/check_manifest.py
)
set_tests_properties(test_goldensuite_manifest PROPERTIES LABELS "golden;manifest")
