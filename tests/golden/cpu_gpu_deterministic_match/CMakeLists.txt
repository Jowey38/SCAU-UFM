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
# G9 stays ci_gate:false until a blocking GPU CI lane exists (M266 exit
# conditions; see the M284 evidence doc), so no gating LABELS golden here.
set_tests_properties(test_cpu_gpu_deterministic_match PROPERTIES LABELS "golden_candidate;cuda;surface2d")
