add_executable(test_coupling_core_state test_coupling_core_state.cpp)
target_link_libraries(test_coupling_core_state
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_core_state COMMAND test_coupling_core_state)

add_executable(test_coupling_adapter_boundaries test_coupling_adapter_boundaries.cpp)
target_link_libraries(test_coupling_adapter_boundaries
    PRIVATE
        scau::coupling_drainage
        scau::coupling_river
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_adapter_boundaries COMMAND test_coupling_adapter_boundaries)

add_executable(test_coupling_shared_exchange test_coupling_shared_exchange.cpp)
target_link_libraries(test_coupling_shared_exchange
    PRIVATE
        scau::coupling_drainage
        scau::coupling_river
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_shared_exchange COMMAND test_coupling_shared_exchange)

add_executable(test_coupling_shared_replay test_coupling_shared_replay.cpp)
target_link_libraries(test_coupling_shared_replay
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_shared_replay COMMAND test_coupling_shared_replay)

add_executable(test_coupling_mock_scheduler test_coupling_mock_scheduler.cpp)
target_link_libraries(test_coupling_mock_scheduler
    PRIVATE
        scau::coupling_drainage
        scau::coupling_river
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_scheduler COMMAND test_coupling_mock_scheduler)

add_executable(test_coupling_engine_report test_coupling_engine_report.cpp)
target_link_libraries(test_coupling_engine_report
    PRIVATE
        scau::coupling_drainage
        scau::coupling_river
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_engine_report COMMAND test_coupling_engine_report)

add_executable(test_coupling_engine_health test_coupling_engine_health.cpp)
target_link_libraries(test_coupling_engine_health
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_engine_health COMMAND test_coupling_engine_health)

add_executable(test_coupling_fault_controller_diagnostic test_coupling_fault_controller_diagnostic.cpp)
target_link_libraries(test_coupling_fault_controller_diagnostic
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_diagnostic COMMAND test_coupling_fault_controller_diagnostic)

add_executable(test_coupling_fault_controller_policy test_coupling_fault_controller_policy.cpp)
target_link_libraries(test_coupling_fault_controller_policy
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_policy COMMAND test_coupling_fault_controller_policy)

add_executable(test_coupling_mock_scheduler_fault_observation test_coupling_mock_scheduler_fault_observation.cpp)
target_link_libraries(test_coupling_mock_scheduler_fault_observation
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_scheduler_fault_observation COMMAND test_coupling_mock_scheduler_fault_observation)

add_executable(test_coupling_mock_scheduler_fault_observation_ordering test_coupling_mock_scheduler_fault_observation_ordering.cpp)
target_link_libraries(test_coupling_mock_scheduler_fault_observation_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_scheduler_fault_observation_ordering COMMAND test_coupling_mock_scheduler_fault_observation_ordering)

add_executable(test_coupling_mock_scheduler_fault_consumption test_coupling_mock_scheduler_fault_consumption.cpp)
target_link_libraries(test_coupling_mock_scheduler_fault_consumption
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_scheduler_fault_consumption COMMAND test_coupling_mock_scheduler_fault_consumption)

add_executable(test_coupling_mock_scheduler_fault_consumption_ordering test_coupling_mock_scheduler_fault_consumption_ordering.cpp)
target_link_libraries(test_coupling_mock_scheduler_fault_consumption_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_scheduler_fault_consumption_ordering COMMAND test_coupling_mock_scheduler_fault_consumption_ordering)

add_executable(test_coupling_fault_controller_passive_state test_coupling_fault_controller_passive_state.cpp)
target_link_libraries(test_coupling_fault_controller_passive_state
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_state COMMAND test_coupling_fault_controller_passive_state)

add_executable(test_coupling_fault_controller_passive_state_ordering test_coupling_fault_controller_passive_state_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_passive_state_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_state_ordering COMMAND test_coupling_fault_controller_passive_state_ordering)

add_executable(test_coupling_fault_controller_passive_transition test_coupling_fault_controller_passive_transition.cpp)
target_link_libraries(test_coupling_fault_controller_passive_transition
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_transition COMMAND test_coupling_fault_controller_passive_transition)

add_executable(test_coupling_fault_controller_passive_transition_ordering test_coupling_fault_controller_passive_transition_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_passive_transition_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_transition_ordering COMMAND test_coupling_fault_controller_passive_transition_ordering)

add_executable(test_coupling_flow_limit test_coupling_flow_limit.cpp)
target_link_libraries(test_coupling_flow_limit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_flow_limit COMMAND test_coupling_flow_limit)

add_executable(test_coupling_mass_deficit_account test_coupling_mass_deficit_account.cpp)
target_link_libraries(test_coupling_mass_deficit_account
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mass_deficit_account COMMAND test_coupling_mass_deficit_account)

add_executable(test_coupling_exchange_decision test_coupling_exchange_decision.cpp)
target_link_libraries(test_coupling_exchange_decision
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_decision COMMAND test_coupling_exchange_decision)

add_executable(test_coupling_drain_split test_coupling_drain_split.cpp)
target_link_libraries(test_coupling_drain_split
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_drain_split COMMAND test_coupling_drain_split)

add_executable(test_coupling_nonnegative_storage_backoff test_coupling_nonnegative_storage_backoff.cpp)
target_link_libraries(test_coupling_nonnegative_storage_backoff
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_nonnegative_storage_backoff COMMAND test_coupling_nonnegative_storage_backoff)

add_executable(test_coupling_exchange_pipeline test_coupling_exchange_pipeline.cpp)
target_link_libraries(test_coupling_exchange_pipeline
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_pipeline COMMAND test_coupling_exchange_pipeline)

add_executable(test_coupling_apply_exchange test_coupling_apply_exchange.cpp)
target_link_libraries(test_coupling_apply_exchange
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_apply_exchange COMMAND test_coupling_apply_exchange)

add_executable(test_coupling_exchange_audit test_coupling_exchange_audit.cpp)
target_link_libraries(test_coupling_exchange_audit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_exchange_audit COMMAND test_coupling_exchange_audit)

add_executable(test_coupling_system_mass_audit test_coupling_system_mass_audit.cpp)
target_link_libraries(test_coupling_system_mass_audit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_system_mass_audit COMMAND test_coupling_system_mass_audit)
