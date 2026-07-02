if(NOT SCAU_EMBED_SWMM)
    message(FATAL_ERROR "G8 real-runtime Golden requires SCAU_EMBED_SWMM=ON")
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/swmm_manhole_overflow.inp DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_swmm_single_pipe_surcharge test_swmm_single_pipe_surcharge.cpp)
target_link_libraries(test_swmm_single_pipe_surcharge
    PRIVATE
        scau::coupling_core
        scau::coupling_drainage
        scau::warnings
        GTest::gtest_main
)
target_compile_definitions(test_swmm_single_pipe_surcharge
    PRIVATE
        SCAU_G8_SWMM_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)
add_test(NAME test_swmm_single_pipe_surcharge COMMAND test_swmm_single_pipe_surcharge)
set_tests_properties(test_swmm_single_pipe_surcharge PROPERTIES LABELS candidate_non_gating)
