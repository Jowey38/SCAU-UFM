add_executable(test_cpu_gpu_deterministic_match
    test_cpu_gpu_deterministic_match.cpp
)
target_link_libraries(test_cpu_gpu_deterministic_match
    PRIVATE
        scau::surface2d
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_cpu_gpu_deterministic_match COMMAND test_cpu_gpu_deterministic_match)
# G9 active gate (M285): the gpu-cuda-golden self-hosted lane runs this
# blocking on the governed GPU host; hosted CUDA-off builds take the
# explicit SKIP path.
set_tests_properties(test_cpu_gpu_deterministic_match PROPERTIES LABELS "golden;cuda;surface2d")
