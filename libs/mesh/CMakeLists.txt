add_library(scau_mesh
    src/mesh.cpp
)
add_library(scau::mesh ALIAS scau_mesh)

target_include_directories(scau_mesh
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_mesh
    PUBLIC
        scau::core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_mesh PUBLIC cxx_std_20)
