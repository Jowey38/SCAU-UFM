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
