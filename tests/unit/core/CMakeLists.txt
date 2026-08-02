add_executable(test_core_types test_types.cpp)
target_link_libraries(test_core_types
    PRIVATE
        scau::core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_core_types COMMAND test_core_types)

add_executable(test_core_error test_error.cpp)
target_link_libraries(test_core_error
    PRIVATE
        scau::core
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_core_error COMMAND test_core_error)

add_executable(test_sim_driver test_sim_driver.cpp)
target_link_libraries(test_sim_driver
    PRIVATE
        scau::sim_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_sim_driver COMMAND test_sim_driver)
