find_package(netCDF CONFIG REQUIRED)

add_library(scau_stcf
    src/schema.cpp
    src/validate.cpp
    src/topology.cpp
    src/io_netcdf.cpp
    src/io_ugrid.cpp
)
add_library(scau::stcf ALIAS scau_stcf)

target_include_directories(scau_stcf
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_stcf
    PUBLIC
        scau::core
    PRIVATE
        scau::warnings
        netCDF::netcdf
)

target_compile_features(scau_stcf PUBLIC cxx_std_20)
