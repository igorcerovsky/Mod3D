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

find_package(nlohmann_json 3.10 QUIET)
if(NOT nlohmann_json_FOUND AND NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

if(NOT TARGET pfld)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../pfld")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../pfld" pfld)
    endif()
endif()

# Header-only independent inversion physics and inference interface library
add_library(inv INTERFACE)

target_link_libraries(inv INTERFACE pfld Threads::Threads nlohmann_json::nlohmann_json)

target_include_directories(inv INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/inv>
    $<INSTALL_INTERFACE:include>
)

# Optional ONNX Runtime support for 3D Swin Transformer neural inference
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ONNXRUNTIME QUIET IMPORTED_TARGET libonnxruntime)
endif()

if(TARGET PkgConfig::ONNXRUNTIME)
    message(STATUS "ONNX Runtime found via pkg-config (imported target)")
    target_compile_definitions(inv INTERFACE MOD3D_HAS_ONNX=1)
    target_link_libraries(inv INTERFACE PkgConfig::ONNXRUNTIME)
else()
    find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
        HINTS /opt/homebrew/include/onnxruntime /usr/local/include/onnxruntime
              /opt/homebrew/Cellar/onnxruntime/*/include/onnxruntime)
    find_library(ONNXRUNTIME_LIBRARY onnxruntime
        HINTS /opt/homebrew/lib /usr/local/lib
              /opt/homebrew/Cellar/onnxruntime/*/lib)
    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY)
        message(STATUS "ONNX Runtime found via find_library: ${ONNXRUNTIME_LIBRARY}")
        target_compile_definitions(inv INTERFACE MOD3D_HAS_ONNX=1)
        target_include_directories(inv INTERFACE ${ONNXRUNTIME_INCLUDE_DIR})
        target_link_libraries(inv INTERFACE ${ONNXRUNTIME_LIBRARY})
    else()
        message(STATUS "ONNX Runtime NOT found - AI neural inference disabled")
    endif()
endif()

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
