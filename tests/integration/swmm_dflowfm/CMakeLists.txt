add_executable(test_tri_coupling_multistep test_tri_coupling_multistep.cpp)
target_link_libraries(test_tri_coupling_multistep
    PRIVATE
        scau::coupling_driver
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_tri_coupling_multistep COMMAND test_tri_coupling_multistep)
