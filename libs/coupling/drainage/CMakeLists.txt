add_library(scau_coupling_drainage
    src/swmm_boundary.cpp
)
add_library(scau::coupling_drainage ALIAS scau_coupling_drainage)

if(SCAU_EMBED_SWMM)
    target_sources(scau_coupling_drainage PRIVATE src/swmm_adapter/swmm_engine.cpp)
endif()

target_include_directories(scau_coupling_drainage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(scau_coupling_drainage
    PUBLIC
        scau::coupling_core
    PRIVATE
        scau::warnings
)

if(SCAU_EMBED_SWMM)
    target_link_libraries(scau_coupling_drainage PRIVATE scau::extern_swmm5)
    target_compile_definitions(scau_coupling_drainage PUBLIC SCAU_HAS_SWMM5=1)
endif()

target_compile_features(scau_coupling_drainage PUBLIC cxx_std_20)
