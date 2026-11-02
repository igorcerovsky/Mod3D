cmake_minimum_required(VERSION 3.20)
project(inv VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Avoid CMP0135 warning in CMake 3.24+ for FetchContent
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

find_package(Threads REQUIRED)

# Header-only independent inversion physics and inference interface library
add_library(inv INTERFACE)

target_link_libraries(inv INTERFACE Threads::Threads)

target_include_directories(inv INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/inv>
    $<INSTALL_INTERFACE:include>
)

# Standalone application
add_executable(inv_app
    app/inv_app.cpp
)

target_include_directories(inv_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/inv
)

target_link_libraries(inv_app PRIVATE inv Threads::Threads)

enable_testing()
add_subdirectory(tests)
