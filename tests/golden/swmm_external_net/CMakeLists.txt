add_executable(test_swmm_external_net test_swmm_external_net.cpp)
target_link_libraries(test_swmm_external_net
    PRIVATE scau::coupling_drainage scau::warnings GTest::gtest_main)
target_compile_definitions(test_swmm_external_net
    PRIVATE SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cases")
add_test(NAME test_swmm_external_net COMMAND test_swmm_external_net)
set_tests_properties(test_swmm_external_net PROPERTIES LABELS candidate_non_gating)
