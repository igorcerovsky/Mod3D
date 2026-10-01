cmake_minimum_required(VERSION 3.20)
project(pfld VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Avoid CMP0135 warning in CMake 3.24+ for FetchContent
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Modern compiler warnings & cross-platform math precision
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -ffp-contract=off)
endif()

find_package(Threads REQUIRED)

# Header-only library
add_library(pfld INTERFACE)

target_link_libraries(pfld INTERFACE Threads::Threads)

target_include_directories(pfld INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/pfld>
    $<INSTALL_INTERFACE:include>
)

# Standalone application
add_executable(pfld_app
    app/pfld_app.cpp
    tests/pfld_test_io.cpp
)

target_include_directories(pfld_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/pfld
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

target_link_libraries(pfld_app PRIVATE pfld Threads::Threads)

# Copy data files for pfld_app to current binary directory
configure_file(tests/test_data/pfld_facets.txt ${CMAKE_CURRENT_BINARY_DIR}/pfld_facets.txt COPYONLY)
configure_file(tests/test_data/pfld_points.txt ${CMAKE_CURRENT_BINARY_DIR}/pfld_points.txt COPYONLY)
configure_file(tests/test_data/pfld_results.txt ${CMAKE_CURRENT_BINARY_DIR}/pfld_results.txt COPYONLY)

enable_testing()
add_subdirectory(tests)
