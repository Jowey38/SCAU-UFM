add_library(scau_coupling_drainage
    src/mock_swmm_engine.cpp
    src/roof_drainage_adapter.cpp
    src/swmm_engine.cpp
)
add_library(scau::coupling_drainage ALIAS scau_coupling_drainage)

target_include_directories(scau_coupling_drainage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_drainage
    PUBLIC
        scau::core
        scau::surface2d
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_drainage PUBLIC cxx_std_20)
