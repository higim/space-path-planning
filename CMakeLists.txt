cmake_minimum_required(VERSION 3.28)

include(FetchContent)

# Headers are included unprefixed (e.g. "grid.h"), so every subdir is a search path.
include_directories(
    include
    include/core
    include/maps
    include/planners
    tests
    apps
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

project(SpacePathPlanning)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)

enable_testing()
include(GoogleTest)   # provides gtest_discover_tests()

# Compiled (non-header-only) sources live here. Add future .cpp files to this list.
add_library(planning
    src/maps/grid.cpp
    src/maps/search_trace.cpp
)

# Demo: build a Mars grid, run A*, and write a JSON search trace.
add_executable(a_star_demo apps/a_star_demo.cpp)
target_link_libraries(a_star_demo planning)

add_executable(priority_list_test tests/test_priority_list.cpp)
target_link_libraries(priority_list_test planning gtest gtest_main)
gtest_discover_tests(priority_list_test)

add_executable(a_star_test tests/test_a_star.cpp)
target_link_libraries(a_star_test planning gtest gtest_main)
gtest_discover_tests(a_star_test)

add_executable(grid_test tests/test_grid.cpp)
target_link_libraries(grid_test planning gtest gtest_main)
gtest_discover_tests(grid_test)