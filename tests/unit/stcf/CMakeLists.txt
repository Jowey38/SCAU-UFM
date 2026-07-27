find_package(netCDF CONFIG REQUIRED)

add_executable(test_stcf_schema test_stcf_schema.cpp)
target_link_libraries(test_stcf_schema
    PRIVATE
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_stcf_schema COMMAND test_stcf_schema)

add_executable(test_stcf_validate test_stcf_validate.cpp)
target_link_libraries(test_stcf_validate
    PRIVATE
        scau::stcf
        scau::warnings
        GTest::gtest_main
)
add_test(NAME test_stcf_validate COMMAND test_stcf_validate)

add_executable(test_stcf_io_netcdf test_stcf_io_netcdf.cpp)
target_link_libraries(test_stcf_io_netcdf
    PRIVATE
        scau::stcf
        scau::warnings
        GTest::gtest_main
        netCDF::netcdf
)
add_test(NAME test_stcf_io_netcdf COMMAND test_stcf_io_netcdf)
