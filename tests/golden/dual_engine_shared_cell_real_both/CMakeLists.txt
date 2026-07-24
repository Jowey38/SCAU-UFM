if(NOT SCAU_EMBED_SWMM)
    message(FATAL_ERROR "G16 dual-real shared-cell Golden requires SCAU_EMBED_SWMM=ON")
endif()
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(test_dual_engine_shared_cell_real_both test_dual_engine_shared_cell_real_both.cpp)
target_link_libraries(test_dual_engine_shared_cell_real_both
    PRIVATE scau::coupling_driver scau::coupling_drainage scau::coupling_river scau::warnings GTest::gtest_main)
target_compile_definitions(test_dual_engine_shared_cell_real_both
    PRIVATE SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases")
add_test(NAME test_dual_engine_shared_cell_real_both COMMAND test_dual_engine_shared_cell_real_both)
set_tests_properties(test_dual_engine_shared_cell_real_both PROPERTIES LABELS "candidate_non_gating")
