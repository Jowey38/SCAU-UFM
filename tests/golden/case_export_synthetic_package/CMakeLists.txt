add_executable(test_case_export_synthetic_package
    test_case_export_synthetic_package.cpp
)
target_compile_definitions(test_case_export_synthetic_package
    PRIVATE SCAU_CASE_EXPORT_PACKAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/cases")
target_link_libraries(test_case_export_synthetic_package
    PRIVATE
        scau::sim_driver
        scau::coupling_drainage
        scau::coupling_river
        scau::surface2d
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_case_export_synthetic_package COMMAND test_case_export_synthetic_package)
set_tests_properties(test_case_export_synthetic_package PROPERTIES LABELS golden)
