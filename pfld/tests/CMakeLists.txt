include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Copy test data files to build directory
file(GLOB TEST_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test_data/*")
file(COPY ${TEST_DATA_FILES} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/test_data")

# test_pfld
add_executable(test_pfld unittest_pfld.cpp pfld_test_io.cpp)
target_include_directories(test_pfld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/pfld
)
target_compile_definitions(test_pfld PRIVATE
    PFLD_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_data"
)
target_link_libraries(test_pfld PRIVATE pfld GTest::gtest_main Threads::Threads)
gtest_discover_tests(test_pfld WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

