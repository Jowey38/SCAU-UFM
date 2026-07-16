add_library(scau_coupling_driver
    src/coupling_state_endpoint_provider.cpp
    src/roof_exchange_gate.cpp
    src/roof_swmm_step_driver.cpp
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
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_driver PUBLIC cxx_std_20)
