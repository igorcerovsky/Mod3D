cmake_minimum_required(VERSION 3.20)
project(Mod3D VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Avoid CMP0135 warning in CMake 3.24+ for FetchContent
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Modern compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

find_package(Threads REQUIRED)

# Core static library
add_library(mod3d_core STATIC
    src/Point3D.cpp
    src/PotField.cpp
    src/Body.cpp
    src/Facet3Pt.cpp
    src/FieldCompute.cpp
    src/ColumnPoint.cpp
    src/Grid.cpp
    src/Model.cpp
    src/Inversion.cpp
    src/Observation.cpp
)

target_link_libraries(mod3d_core PUBLIC Threads::Threads)

target_include_directories(mod3d_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

enable_testing()
add_subdirectory(tests)
