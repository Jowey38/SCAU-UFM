add_library(scau_coupling_river
    src/dflowfm_boundary.cpp
)
add_library(scau::coupling_river ALIAS scau_coupling_river)

if(SCAU_ENABLE_DFLOWFM_BMI_RUNTIME)
    target_sources(scau_coupling_river PRIVATE src/dflowfm_adapter/dflowfm_engine.cpp)
    target_compile_definitions(scau_coupling_river PUBLIC SCAU_HAS_DFLOWFM_BMI_RUNTIME=1)
endif()

target_include_directories(scau_coupling_river
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_river
    PUBLIC
        scau::coupling_core
    PRIVATE
        scau::warnings
)

if(SCAU_ENABLE_DFLOWFM_BMI_RUNTIME AND CMAKE_DL_LIBS)
    target_link_libraries(scau_coupling_river PRIVATE ${CMAKE_DL_LIBS})
endif()

target_compile_features(scau_coupling_river PUBLIC cxx_std_20)
