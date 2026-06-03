add_library(scau_coupling_river
    src/dflowfm_boundary.cpp
)
add_library(scau::coupling_river ALIAS scau_coupling_river)

target_include_directories(scau_coupling_river
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_river
    PUBLIC
        scau::coupling_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_river PUBLIC cxx_std_20)
