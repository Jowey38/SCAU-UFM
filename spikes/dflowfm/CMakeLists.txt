cmake_minimum_required(VERSION 3.20)
project(scau_dflowfm_spike LANGUAGES CXX)

# Spike CMake is fully standalone per third-party-spike-design §2.
# Do NOT add_subdirectory(this) from the main repo.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DFLOWFM_SOURCE_DIR "" CACHE PATH "Path to a local Delft3D / D-Flow FM checkout")
if(NOT DFLOWFM_SOURCE_DIR)
    message(FATAL_ERROR
        "DFLOWFM_SOURCE_DIR must point at a local Delft3D-main checkout, e.g. "
        "/path/to/Delft3D-main. The spike does not vendor any third-party "
        "engine source; the caller supplies a built D-Flow FM library and its "
        "BMI header location via this cache variable.")
endif()

# Primary hint: the external checkout. Fallback: the vendored BMI interface
# contract snapshot at extern/dflowfm/include (header contract only; the
# engine library itself must still come from DFLOWFM_SOURCE_DIR).
find_path(DFLOWFM_BMI_INCLUDE_DIR
    NAMES bmi.h
    HINTS
        ${DFLOWFM_SOURCE_DIR}/src/engines_gpl/dimr/packages/dimr_lib/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../extern/dflowfm/include
    REQUIRED
)

find_library(DFLOWFM_LIB
    NAMES dflowfm libdflowfm
    HINTS
        ${DFLOWFM_SOURCE_DIR}/build/lib
        ${DFLOWFM_SOURCE_DIR}/install/lib
        ${DFLOWFM_SOURCE_DIR}/lib
    REQUIRED
)

add_executable(dflowfm_spike_host host/dflowfm_spike_host.cpp)
target_include_directories(dflowfm_spike_host PRIVATE ${DFLOWFM_BMI_INCLUDE_DIR})
target_link_libraries(dflowfm_spike_host PRIVATE ${DFLOWFM_LIB})
