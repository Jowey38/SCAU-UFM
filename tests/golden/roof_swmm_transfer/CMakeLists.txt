if(NOT SCAU_EMBED_SWMM)
    message(FATAL_ERROR "M240-ROOF golden requires SCAU_EMBED_SWMM=ON (real SwmmEngine)")
endif()

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_roof_swmm_transfer test_roof_swmm_transfer.cpp)
target_link_libraries(test_roof_swmm_transfer
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
target_compile_definitions(test_roof_swmm_transfer
    PRIVATE
        SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases"
)
add_test(NAME test_roof_swmm_transfer COMMAND test_roof_swmm_transfer)
set_tests_properties(test_roof_swmm_transfer PROPERTIES LABELS golden)
