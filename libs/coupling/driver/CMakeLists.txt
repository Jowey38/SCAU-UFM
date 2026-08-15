add_library(scau_coupling_driver
    src/checkpoint_coordinator.cpp
    src/checkpoint_payloads.cpp
    src/coupling_state_endpoint_provider.cpp
    src/dflowfm_checkpoint.cpp
    src/dflowfm_external_net_provider.cpp
    src/dflowfm_volume_provider.cpp
    src/roof_exchange_gate.cpp
    src/roof_swmm_step_driver.cpp
    src/surface2d_coupling_map.cpp
    src/tri_coupling.cpp
    src/whole_system_mass_audit.cpp
)
add_library(scau::coupling_driver ALIAS scau_coupling_driver)

target_include_directories(scau_coupling_driver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_driver
    PUBLIC
        scau::coupling_core
        scau::coupling_drainage
        scau::coupling_river
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_driver PUBLIC cxx_std_20)
