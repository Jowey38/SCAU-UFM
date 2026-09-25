add_executable(test_dflowfm_provider_contract test_dflowfm_provider_contract.cpp)
target_compile_definitions(test_dflowfm_provider_contract
    PRIVATE
        SCAU_PYTHON_EXECUTABLE="${Python3_EXECUTABLE}"
        SCAU_PYTHON_PACKAGE_DIR="${PROJECT_SOURCE_DIR}/python"
)
target_link_libraries(test_dflowfm_provider_contract
    PRIVATE
        scau::sim_driver
        scau::coupling_drainage
        scau::coupling_river
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_dflowfm_provider_contract COMMAND test_dflowfm_provider_contract)
set_tests_properties(test_dflowfm_provider_contract PROPERTIES LABELS golden)
