add_executable(test_validation_release_gate_mock_publisher test_validation_release_gate_mock_publisher.cpp)
target_link_libraries(test_validation_release_gate_mock_publisher
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_publisher COMMAND test_validation_release_gate_mock_publisher)

add_executable(test_validation_release_gate_mock_payload test_validation_release_gate_mock_payload.cpp)
target_link_libraries(test_validation_release_gate_mock_payload
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_payload COMMAND test_validation_release_gate_mock_payload)

add_executable(test_validation_release_gate_payload_publish test_validation_release_gate_payload_publish.cpp)
target_link_libraries(test_validation_release_gate_payload_publish
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_payload_publish COMMAND test_validation_release_gate_payload_publish)

add_executable(test_validation_release_gate_chain_aggregate test_validation_release_gate_chain_aggregate.cpp)
target_link_libraries(test_validation_release_gate_chain_aggregate
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_chain_aggregate COMMAND test_validation_release_gate_chain_aggregate)

add_executable(test_validation_release_gate_dependency_guard test_validation_release_gate_dependency_guard.cpp)
target_link_libraries(test_validation_release_gate_dependency_guard
    PRIVATE
        scau::warnings
        GTest::gtest_main
)
target_compile_definitions(test_validation_release_gate_dependency_guard
    PRIVATE
        SCAU_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
add_test(NAME test_validation_release_gate_dependency_guard COMMAND test_validation_release_gate_dependency_guard)

add_executable(test_validation_release_gate_mock_sandbox_sink test_validation_release_gate_mock_sandbox_sink.cpp)
target_link_libraries(test_validation_release_gate_mock_sandbox_sink
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_sandbox_sink COMMAND test_validation_release_gate_mock_sandbox_sink)

add_executable(test_validation_release_gate_mock_sandbox_adapter test_validation_release_gate_mock_sandbox_adapter.cpp)
target_link_libraries(test_validation_release_gate_mock_sandbox_adapter
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_sandbox_adapter COMMAND test_validation_release_gate_mock_sandbox_adapter)

add_executable(test_validation_release_gate_mock_sandbox_stage_record test_validation_release_gate_mock_sandbox_stage_record.cpp)
target_link_libraries(test_validation_release_gate_mock_sandbox_stage_record
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_sandbox_stage_record COMMAND test_validation_release_gate_mock_sandbox_stage_record)

add_executable(test_validation_release_gate_mock_sandbox_stage_aggregate test_validation_release_gate_mock_sandbox_stage_aggregate.cpp)
target_link_libraries(test_validation_release_gate_mock_sandbox_stage_aggregate
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_sandbox_stage_aggregate COMMAND test_validation_release_gate_mock_sandbox_stage_aggregate)

add_executable(test_validation_release_gate_mock_sandbox_final_chain test_validation_release_gate_mock_sandbox_final_chain.cpp)
target_link_libraries(test_validation_release_gate_mock_sandbox_final_chain
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_mock_sandbox_final_chain COMMAND test_validation_release_gate_mock_sandbox_final_chain)

add_executable(test_validation_release_gate_controlled_np_sink test_validation_release_gate_controlled_non_production_publisher_sink.cpp)
target_link_libraries(test_validation_release_gate_controlled_np_sink
    PRIVATE
        scau::validation_release_gate
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_validation_release_gate_controlled_np_sink COMMAND test_validation_release_gate_controlled_np_sink)
