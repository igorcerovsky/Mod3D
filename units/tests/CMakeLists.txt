include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_units_standalone test_units.cpp)
target_link_libraries(test_units_standalone PRIVATE units GTest::gtest_main)
gtest_discover_tests(test_units_standalone)
