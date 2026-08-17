if(NOT SCAU_EMBED_SWMM)
    message(FATAL_ERROR "G19 surface2d tri-coupling Golden requires SCAU_EMBED_SWMM=ON")
endif()
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(test_surface2d_tri_coupling_real test_surface2d_tri_coupling_real.cpp)
target_link_libraries(test_surface2d_tri_coupling_real
    PRIVATE
        scau::sim_driver
        scau::coupling_drainage
        scau::coupling_river
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
target_compile_definitions(test_surface2d_tri_coupling_real
    PRIVATE SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases")
add_test(NAME test_surface2d_tri_coupling_real COMMAND test_surface2d_tri_coupling_real)
set_tests_properties(test_surface2d_tri_coupling_real PROPERTIES LABELS golden)
