add_executable(test_validation_release_gate_mock_publisher test_validation_release_gate_mock_publisher.cpp)
target_link_libraries(test_validation_release_gate_mock_publisher
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_publisher COMMAND test_validation_release_gate_mock_publisher)
