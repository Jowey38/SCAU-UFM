# Builds the vendored SWMM 5.2.4 solver (extern/swmm5/) as a static library.
#
# Target: scau_extern_swmm5 (alias scau::extern_swmm5)
#
# Governance (project-layout-design.md §4.12, third_party/manifest/swmm5.version):
# - extern/swmm5/ holds unmodified upstream source; never edit it in place.
# - This module intentionally does NOT use the upstream src/solver/CMakeLists.txt
#   (which builds a shared lib and hard-requires OpenMP).
# - Third-party code is compiled without scau::warnings and without
#   warnings-as-errors; only consuming adapter code is held to project warnings.
# - Only libs/coupling/drainage/src/*.cpp may include swmm5.h.

# The root project() only enables CXX; the vendored solver is C99.
enable_language(C)

set(SCAU_SWMM5_DIR "${CMAKE_SOURCE_DIR}/extern/swmm5/src/solver")

file(GLOB SCAU_SWMM5_SOURCES "${SCAU_SWMM5_DIR}/*.c")

add_library(scau_extern_swmm5 STATIC ${SCAU_SWMM5_SOURCES})
add_library(scau::extern_swmm5 ALIAS scau_extern_swmm5)

target_include_directories(scau_extern_swmm5
    PUBLIC
        $<BUILD_INTERFACE:${SCAU_SWMM5_DIR}/include>
    PRIVATE
        ${SCAU_SWMM5_DIR}
)

# Static embedding (same-process, one standard library boundary per the
# CLAUDE.md Phase 1/2 constraint). The upstream dllexport decoration in
# swmm5.h is harmless in a static archive linked into our binaries.
target_compile_definitions(scau_extern_swmm5
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>
)

# OpenMP is optional here (upstream makes it REQUIRED); without it the
# `#pragma omp` lines in dynwave.c are inert.
find_package(OpenMP COMPONENTS C QUIET)
if(OpenMP_C_FOUND)
    target_link_libraries(scau_extern_swmm5 PRIVATE OpenMP::OpenMP_C)
endif()

# Third-party code: keep the build quiet instead of patching upstream.
if(MSVC)
    target_compile_options(scau_extern_swmm5 PRIVATE /W0)
else()
    target_compile_options(scau_extern_swmm5 PRIVATE -w)
endif()

set_target_properties(scau_extern_swmm5 PROPERTIES
    C_STANDARD 99
    POSITION_INDEPENDENT_CODE ON
    FOLDER "extern"
)
