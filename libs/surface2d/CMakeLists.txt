add_library(scau_surface2d
    src/audit/mass.cpp
    src/backend.cpp
    src/boundary/conditions.cpp
    src/cfl/diagnostics.cpp
    src/dpm/closure_laws.cpp
    src/dpm/cvc_augmented_flux.cpp
    src/dpm/edge_conveyance.cpp
    src/dpm/fields.cpp
    src/dpm/phi_t_remap.cpp
    src/dpm/tensor_projection.cpp
    src/geometry/cache.cpp
    src/source_terms/coupling_exchange.cpp
    src/source_terms/fields.cpp
    src/source_terms/friction.cpp
    src/source_terms/infiltration.cpp
    src/source_terms/rainfall.cpp
    src/source_terms/runoff/fields.cpp
    src/source_terms/runoff/green_ampt.cpp
    src/source_terms/runoff/result.cpp
    src/source_terms/runoff/runoff_generation.cpp
    src/source_terms/runoff/soil_params.cpp
    src/source_terms/runoff/roof_step.cpp
    src/source_terms/runoff/step_inputs.cpp
    src/stcf_bridge/assemble.cpp
    src/stcf_bridge/load_case.cpp
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
        scau::stcf
    PRIVATE
        scau::warnings
)

target_compile_features(scau_surface2d PUBLIC cxx_std_20)

if(SCAU_ENABLE_CUDA)
    # Separate target so scau::warnings' MSVC/GCC flags never reach nvcc.
    # --fmad=false keeps device contraction identical to the /fp:precise
    # host, which is what makes the shared SCAU_HD numerics CPU-matchable
    # (M280 pattern / M284 plan).
    add_library(scau_surface2d_cuda STATIC
        src/backends/cuda/cuda_backend.cu
    )
    target_include_directories(scau_surface2d_cuda
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(scau_surface2d_cuda
        PUBLIC
            scau::core
            scau::mesh
    )
    set_target_properties(scau_surface2d_cuda PROPERTIES
        CUDA_ARCHITECTURES "${SCAU_CUDA_ARCHITECTURES}"
        CUDA_STANDARD 20
        CUDA_STANDARD_REQUIRED ON
    )
    target_compile_options(scau_surface2d_cuda PRIVATE
        $<$<COMPILE_LANGUAGE:CUDA>:--fmad=false>
    )
    target_compile_definitions(scau_surface2d PUBLIC SCAU_SURFACE2D_HAS_CUDA)
    target_link_libraries(scau_surface2d PRIVATE scau_surface2d_cuda)
endif()
