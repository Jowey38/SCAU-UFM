cmake_minimum_required(VERSION 3.27)

project(SCAU_UFM
    VERSION 0.1.0
    DESCRIPTION "SCAU urban flood model"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(ScauWarnings)

option(SCAU_EMBED_SWMM "Build the vendored SWMM 5.2.4 solver and the real SwmmEngine adapter" ON)

if(SCAU_EMBED_SWMM)
    include(${CMAKE_SOURCE_DIR}/cmake/third_party/swmm.cmake)
endif()

enable_testing()

add_subdirectory(libs/core)
add_subdirectory(libs/mesh)
add_subdirectory(libs/surface2d)
add_subdirectory(libs/coupling/core)
add_subdirectory(libs/coupling/drainage)
add_subdirectory(tests)
