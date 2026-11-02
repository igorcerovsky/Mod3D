include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_inv unittest_inv.cpp)
target_include_directories(test_inv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/inv
)
target_link_libraries(test_inv PRIVATE inv GTest::gtest_main Threads::Threads)
gtest_discover_tests(test_inv WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
