add_library(scau_sim_driver
    run_loop.cpp
    run_summary.cpp
    runtime_config_io.cpp
    sim_driver.cpp
)
add_library(scau::sim_driver ALIAS scau_sim_driver)

target_include_directories(scau_sim_driver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(scau_sim_driver
    PUBLIC
        scau::coupling_drainage
        scau::coupling_driver
        scau::coupling_river
        scau::stcf
        scau::surface2d
    PRIVATE
        scau::warnings
)
target_compile_features(scau_sim_driver PUBLIC cxx_std_20)

add_executable(scau_sim main.cpp)
target_link_libraries(scau_sim PRIVATE scau::sim_driver scau::warnings)
target_compile_features(scau_sim PRIVATE cxx_std_20)
