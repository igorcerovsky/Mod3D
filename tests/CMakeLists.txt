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
    PFLD_APP_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../pfld_app"
    LEGACY_SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../legacy/sample_data"
)

# Test Point3D
add_executable(test_point3d test_point3d.cpp)
target_link_libraries(test_point3d PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_point3d)

# Test pfld ground truth
add_executable(test_pfld_groundtruth test_pfld_groundtruth.cpp)
target_link_libraries(test_pfld_groundtruth PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_pfld_groundtruth)

# Test FieldCompute (Parallel / Serial Engine)
add_executable(test_field_compute test_field_compute.cpp)
target_link_libraries(test_field_compute PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_field_compute)

# Test Model, ColumnPoint, Grid
add_executable(test_model test_model.cpp)
target_link_libraries(test_model PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_model)

# Test Primitives (Point3D edge cases, Body, ColumnPoint)
add_executable(test_primitives test_primitives.cpp)
target_link_libraries(test_primitives PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_primitives)

# Test Grid (rotations, statistics, operators, Surfer binary)
add_executable(test_grid test_grid.cpp)
target_link_libraries(test_grid PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_grid)

# Test Potential Field Tensor & Magnetics (Laplace invariance, AmbientField, Magnetization, Linear parallel)
add_executable(test_potential_tensor test_potential_tensor.cpp)
target_link_libraries(test_potential_tensor PRIVATE mod3d_core GTest::gtest_main)
gtest_discover_tests(test_potential_tensor)


