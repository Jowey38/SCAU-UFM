find_package(GTest CONFIG REQUIRED)

add_subdirectory(unit/core)
add_subdirectory(unit/mesh)
add_subdirectory(unit/surface2d)
add_subdirectory(unit/coupling)
add_subdirectory(unit/validation/release_gate)
