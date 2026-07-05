include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(runoff_urban_block)

add_test(
    NAME test_goldensuite_manifest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/suite_manifest/check_manifest.py
)
set_tests_properties(test_goldensuite_manifest PROPERTIES LABELS "golden;manifest")
