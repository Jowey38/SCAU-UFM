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

add_executable(test_coupling_fault_controller_passive_action_audit test_coupling_fault_controller_passive_action_audit.cpp)
target_link_libraries(test_coupling_fault_controller_passive_action_audit
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_action_audit COMMAND test_coupling_fault_controller_passive_action_audit)

add_executable(test_coupling_fault_controller_passive_action_audit_ordering test_coupling_fault_controller_passive_action_audit_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_passive_action_audit_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_action_audit_ordering COMMAND test_coupling_fault_controller_passive_action_audit_ordering)

add_executable(test_coupling_fault_controller_passive_action_outcome test_coupling_fault_controller_passive_action_outcome.cpp)
target_link_libraries(test_coupling_fault_controller_passive_action_outcome
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_action_outcome COMMAND test_coupling_fault_controller_passive_action_outcome)

add_executable(test_coupling_fault_controller_passive_action_outcome_ordering test_coupling_fault_controller_passive_action_outcome_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_passive_action_outcome_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_passive_action_outcome_ordering COMMAND test_coupling_fault_controller_passive_action_outcome_ordering)

add_executable(test_coupling_fault_controller_blocked_action test_coupling_fault_controller_blocked_action.cpp)
target_link_libraries(test_coupling_fault_controller_blocked_action
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_blocked_action COMMAND test_coupling_fault_controller_blocked_action)

add_executable(test_coupling_fault_controller_blocked_action_ordering test_coupling_fault_controller_blocked_action_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_blocked_action_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_blocked_action_ordering COMMAND test_coupling_fault_controller_blocked_action_ordering)

add_executable(test_coupling_fault_controller_scheduler_control_request test_coupling_fault_controller_scheduler_control_request.cpp)
target_link_libraries(test_coupling_fault_controller_scheduler_control_request
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_scheduler_control_request COMMAND test_coupling_fault_controller_scheduler_control_request)

add_executable(test_coupling_fault_controller_scheduler_control_request_ordering test_coupling_fault_controller_scheduler_control_request_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_scheduler_control_request_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_scheduler_control_request_ordering COMMAND test_coupling_fault_controller_scheduler_control_request_ordering)

add_executable(test_coupling_fault_controller_scheduler_control_result test_coupling_fault_controller_scheduler_control_result.cpp)
target_link_libraries(test_coupling_fault_controller_scheduler_control_result
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_scheduler_control_result COMMAND test_coupling_fault_controller_scheduler_control_result)

add_executable(test_coupling_fault_controller_scheduler_control_result_ordering test_coupling_fault_controller_scheduler_control_result_ordering.cpp)
target_link_libraries(test_coupling_fault_controller_scheduler_control_result_ordering
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_fault_controller_scheduler_control_result_ordering COMMAND test_coupling_fault_controller_scheduler_control_result_ordering)

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

add_executable(test_coupling_mock_swmm_engine test_coupling_mock_swmm_engine.cpp)
target_link_libraries(test_coupling_mock_swmm_engine
    PRIVATE
        scau::coupling_drainage
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_mock_swmm_engine COMMAND test_coupling_mock_swmm_engine)

add_executable(test_coupling_roof_drainage_adapter test_coupling_roof_drainage_adapter.cpp)
target_link_libraries(test_coupling_roof_drainage_adapter
    PRIVATE
        scau::coupling_drainage
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_roof_drainage_adapter COMMAND test_coupling_roof_drainage_adapter)

add_executable(test_coupling_roof_exchange_gate test_coupling_roof_exchange_gate.cpp)
target_link_libraries(test_coupling_roof_exchange_gate
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_roof_exchange_gate COMMAND test_coupling_roof_exchange_gate)

add_executable(test_coupling_roof_swmm_step_driver test_coupling_roof_swmm_step_driver.cpp)
target_link_libraries(test_coupling_roof_swmm_step_driver
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_roof_swmm_step_driver COMMAND test_coupling_roof_swmm_step_driver)

add_executable(test_coupling_state_endpoint_provider test_coupling_state_endpoint_provider.cpp)
target_link_libraries(test_coupling_state_endpoint_provider
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_state_endpoint_provider COMMAND test_coupling_state_endpoint_provider)

add_executable(test_coupling_roof_rollback test_coupling_roof_rollback.cpp)
target_link_libraries(test_coupling_roof_rollback
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_roof_rollback COMMAND test_coupling_roof_rollback)

if(SCAU_EMBED_SWMM)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/cases DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

    add_executable(test_coupling_swmm_engine test_coupling_swmm_engine.cpp)
    target_link_libraries(test_coupling_swmm_engine
        PRIVATE
            scau::coupling_drainage
            scau::warnings
            GTest::gtest_main
    )
    target_compile_definitions(test_coupling_swmm_engine
        PRIVATE
            SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases"
    )
    add_test(NAME test_coupling_swmm_engine COMMAND test_coupling_swmm_engine)

    add_executable(test_coupling_roof_swmm_real test_coupling_roof_swmm_real.cpp)
    target_link_libraries(test_coupling_roof_swmm_real
        PRIVATE
            scau::coupling_drainage
            scau::warnings
            GTest::gtest_main
    )
    target_compile_definitions(test_coupling_roof_swmm_real
        PRIVATE
            SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases"
    )
    add_test(NAME test_coupling_roof_swmm_real COMMAND test_coupling_roof_swmm_real)

    add_executable(test_coupling_roof_qlimit_swmm_real test_coupling_roof_qlimit_swmm_real.cpp)
    target_link_libraries(test_coupling_roof_qlimit_swmm_real
        PRIVATE
            scau::coupling_driver
            scau::warnings
            GTest::gtest_main
    )
    target_compile_definitions(test_coupling_roof_qlimit_swmm_real
        PRIVATE
            SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases"
    )
    add_test(NAME test_coupling_roof_qlimit_swmm_real COMMAND test_coupling_roof_qlimit_swmm_real)

    add_executable(test_coupling_roof_rollback_swmm_real test_coupling_roof_rollback_swmm_real.cpp)
    target_link_libraries(test_coupling_roof_rollback_swmm_real
        PRIVATE
            scau::coupling_driver
            scau::warnings
            GTest::gtest_main
    )
    target_compile_definitions(test_coupling_roof_rollback_swmm_real
        PRIVATE
            SCAU_SWMM_TEST_CASE_DIR="${CMAKE_CURRENT_BINARY_DIR}/cases"
    )
    add_test(NAME test_coupling_roof_rollback_swmm_real COMMAND test_coupling_roof_rollback_swmm_real)
endif()

if(SCAU_ENABLE_DFLOWFM_BMI_RUNTIME)
    add_library(fake_dflowfm_bmi SHARED fake_dflowfm_bmi.cpp)
    target_compile_features(fake_dflowfm_bmi PRIVATE cxx_std_20)

    add_library(fake_dflowfm_bmi_missing_symbol SHARED fake_dflowfm_bmi_missing_symbol.cpp)
    target_compile_features(fake_dflowfm_bmi_missing_symbol PRIVATE cxx_std_20)

    add_executable(test_coupling_dflowfm_engine test_coupling_dflowfm_engine.cpp)
    target_link_libraries(test_coupling_dflowfm_engine
        PRIVATE
            scau::coupling_river
            scau::warnings
            GTest::gtest_main
    )
    target_compile_definitions(test_coupling_dflowfm_engine
        PRIVATE
            SCAU_FAKE_DFLOWFM_BMI_LIBRARY="$<TARGET_FILE:fake_dflowfm_bmi>"
            SCAU_FAKE_DFLOWFM_BMI_MISSING_SYMBOL_LIBRARY="$<TARGET_FILE:fake_dflowfm_bmi_missing_symbol>"
    )
    add_dependencies(test_coupling_dflowfm_engine fake_dflowfm_bmi fake_dflowfm_bmi_missing_symbol)
    add_test(NAME test_coupling_dflowfm_engine COMMAND test_coupling_dflowfm_engine)
endif()

add_executable(test_coupling_engine_interface_exchange test_coupling_engine_interface_exchange.cpp)
target_link_libraries(test_coupling_engine_interface_exchange
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_engine_interface_exchange COMMAND test_coupling_engine_interface_exchange)

add_executable(test_coupling_return_exchange test_coupling_return_exchange.cpp)
target_link_libraries(test_coupling_return_exchange
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_return_exchange COMMAND test_coupling_return_exchange)

add_executable(test_coupling_head_driven_exchange test_coupling_head_driven_exchange.cpp)
target_link_libraries(test_coupling_head_driven_exchange
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_head_driven_exchange COMMAND test_coupling_head_driven_exchange)

add_executable(test_coupling_tri_driver test_coupling_tri_driver.cpp)
target_link_libraries(test_coupling_tri_driver
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_tri_driver COMMAND test_coupling_tri_driver)

add_executable(test_coupling_dflowfm_checkpoint test_coupling_dflowfm_checkpoint.cpp)
target_link_libraries(test_coupling_dflowfm_checkpoint
    PRIVATE scau::coupling_driver scau::warnings GTest::gtest_main)
add_test(NAME test_coupling_dflowfm_checkpoint COMMAND test_coupling_dflowfm_checkpoint)
