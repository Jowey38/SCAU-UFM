if(NOT SCAU_EMBED_SWMM)
    message(FATAL_ERROR "real tri-coupling Golden requires SCAU_EMBED_SWMM=ON")
endif()
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(test_tri_coupling_real_minimal test_tri_coupling_real_minimal.cpp)
target_link_libraries(test_tri_coupling_real_minimal
    PRIVATE scau::coupling_driver scau::coupling_drainage scau::coupling_river scau::warnings GTest::gtest_main)
target_compile_definitions(test_tri_coupling_real_minimal
    PRIVATE SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases")
add_test(NAME test_tri_coupling_real_minimal COMMAND test_tri_coupling_real_minimal)
set_tests_properties(test_tri_coupling_real_minimal PROPERTIES LABELS "candidate_non_gating")
