add_library(scau_coupling_core
    src/state.cpp
)
add_library(scau::coupling_core ALIAS scau_coupling_core)

target_include_directories(scau_coupling_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_coupling_core PUBLIC cxx_std_20)
