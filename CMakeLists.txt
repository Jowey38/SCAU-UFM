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

enable_testing()

add_subdirectory(libs/core)
add_subdirectory(tests)
