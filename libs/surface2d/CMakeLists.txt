add_library(scau_surface2d
    src/time_integration/step.cpp
)
add_library(scau::surface2d ALIAS scau_surface2d)

target_include_directories(scau_surface2d
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_surface2d
    PUBLIC
        scau::core
        scau::mesh
    PRIVATE
        scau::warnings
)

target_compile_features(scau_surface2d PUBLIC cxx_std_20)
