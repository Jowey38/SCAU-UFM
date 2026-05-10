add_library(scau_core
    src/error.cpp
)
add_library(scau::core ALIAS scau_core)

target_include_directories(scau_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_core PUBLIC cxx_std_20)
