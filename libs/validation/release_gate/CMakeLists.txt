add_library(scau_validation_release_gate
    src/mock_publisher.cpp
)
add_library(scau::validation_release_gate ALIAS scau_validation_release_gate)

target_include_directories(scau_validation_release_gate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_validation_release_gate
    PUBLIC
        scau::coupling_core
    PRIVATE
        scau::warnings
)

target_compile_features(scau_validation_release_gate PUBLIC cxx_std_20)
