add_executable(test_coupling_core_state test_coupling_core_state.cpp)
target_link_libraries(test_coupling_core_state
    PRIVATE
        scau::coupling_core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_coupling_core_state COMMAND test_coupling_core_state)

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
endif()
