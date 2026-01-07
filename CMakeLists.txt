cmake_minimum_required(VERSION 3.16)
project(movie_booking LANGUAGES CXX)

# Choose the standard: 11, 14, 17
set(CXX_STD "17" CACHE STRING "C++ standard to use (11/14/17)")
set_property(CACHE CXX_STD PROPERTY STRINGS 11 14 17)

# Flag to enable code coverage reports
option(ENABLE_COVERAGE "Enable coverage flags" OFF)

if(ENABLE_COVERAGE)
  # Coverage flags for GCC/Clang
  add_compile_options(-O0 -g --coverage)
  add_link_options(--coverage)
endif()

# -------------------------
# Production library
# -------------------------
add_library(booking
    src/booking_service.cpp
)
target_include_directories(booking PUBLIC include)

# Enforce selected C++ standard
target_compile_features(booking PUBLIC cxx_std_${CXX_STD})
set_target_properties(booking PROPERTIES
    CXX_EXTENSIONS OFF
)

# CLI app
add_executable(booking_cli src/cli_main.cpp)
target_link_libraries(booking_cli PRIVATE booking)

# -------------------------
# Tests (GoogleTest)
# -------------------------
include(CTest)          # sets up CTest integration
enable_testing()

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Recommended by Google on some platforms; harmless on Linux too.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Your gtest file (change path if your folder is "tests/" not "test/")
add_executable(booking_tests
    test/booking_service_tests.cpp
)
target_link_libraries(booking_tests
    PRIVATE booking GTest::gtest_main
)

# Make sure tests compile with the same chosen standard as the library
target_compile_features(booking_tests PRIVATE cxx_std_${CXX_STD})

include(GoogleTest)
gtest_discover_tests(booking_tests)

