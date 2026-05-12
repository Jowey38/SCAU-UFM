add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_link_libraries(test_mesh_geometry
    PRIVATE
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_mesh_geometry COMMAND test_mesh_geometry)

add_executable(test_mesh_validation test_mesh_validation.cpp)
target_link_libraries(test_mesh_validation
    PRIVATE
        scau::mesh
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_mesh_validation COMMAND test_mesh_validation)
