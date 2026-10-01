include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Define reference data path for tests
add_compile_definitions(
    PFLD_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../pfld_UnitTest/test_data"
    LEGACY_SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../legacy/sample_data"
)

# Test Point3D
add_executable(test_point3d test_point3d.cpp)
target_link_libraries(test_point3d PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_point3d)
