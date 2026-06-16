add_library(scau_surface2d
    src/boundary/conditions.cpp
    src/cfl/diagnostics.cpp
    src/dpm/closure_laws.cpp
    src/dpm/edge_classification.cpp
    src/dpm/edge_conveyance.cpp
    src/dpm/fields.cpp
    src/dpm/tensor_projection.cpp
    src/geometry/cache.cpp
    src/reconstruction/hydrostatic.cpp
    src/riemann/hllc.cpp
    src/source_terms/coupling_exchange.cpp
    src/source_terms/fields.cpp
    src/source_terms/friction.cpp
    src/source_terms/infiltration.cpp
    src/source_terms/phi_t.cpp
    src/source_terms/rainfall.cpp
    src/source_terms/runoff/fields.cpp
    src/source_terms/runoff/green_ampt.cpp
    src/source_terms/runoff/result.cpp
    src/source_terms/runoff/runoff_generation.cpp
    src/source_terms/runoff/soil_params.cpp
    src/time_integration/step.cpp
    src/wetting_drying/limits.cpp
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
