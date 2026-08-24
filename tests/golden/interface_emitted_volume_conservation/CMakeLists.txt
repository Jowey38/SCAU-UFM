add_executable(test_interface_emitted_volume_conservation
    test_interface_emitted_volume_conservation.cpp
)
target_link_libraries(test_interface_emitted_volume_conservation
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_interface_emitted_volume_conservation COMMAND test_interface_emitted_volume_conservation)
set_tests_properties(test_interface_emitted_volume_conservation PROPERTIES LABELS golden)
