cmake_minimum_required(VERSION 3.20)
project(scau_swmm_spike LANGUAGES CXX)

# Spike CMake is fully standalone per third-party-spike-design §2:
#   "spikes/ 不得与 libs/ extern/ third_party/ apps/ 共享 CMake target"
# Do NOT add_subdirectory(this) from the main repo CMakeLists.txt.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SWMM_SOURCE_DIR "" CACHE PATH "Path to a local SWMM 5.2.4 checkout")
set(SWMM_BUILD_DIR "" CACHE PATH "Optional path to a separately configured SWMM build tree")
if(NOT SWMM_SOURCE_DIR)
    message(FATAL_ERROR
        "SWMM_SOURCE_DIR must point at a local SWMM 5.2.4 checkout, e.g. "
        "/path/to/Stormwater-Management-Model-develop. Build SWMM separately "
        "and point this variable at it; the spike will only consume the public "
        "swmm5.h and link against the built libswmm5.")
endif()

find_path(SWMM_INCLUDE_DIR
    NAMES swmm5.h
    HINTS ${SWMM_SOURCE_DIR}/src/solver/include
    REQUIRED
)

find_library(SWMM_SOLVER_LIB
    NAMES swmm5 libswmm5
    HINTS
        ${SWMM_BUILD_DIR}/src/solver
        ${SWMM_BUILD_DIR}/src/solver/Release
        ${SWMM_BUILD_DIR}/src/solver/Debug
        ${SWMM_SOURCE_DIR}/build/src/solver
        ${SWMM_SOURCE_DIR}/build/src/solver/Release
        ${SWMM_SOURCE_DIR}/build/src/solver/Debug
        ${SWMM_SOURCE_DIR}/install/lib
    REQUIRED
)

add_executable(swmm_spike_host host/swmm_spike_host.cpp)
target_include_directories(swmm_spike_host PRIVATE ${SWMM_INCLUDE_DIR})
target_link_libraries(swmm_spike_host PRIVATE ${SWMM_SOLVER_LIB})
