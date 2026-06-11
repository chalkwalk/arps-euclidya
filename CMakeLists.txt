# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.22)

# Set the project name
project(ArpsEuclidya VERSION 0.5.0)

option(ARPS_BUILD_TESTS "Build unit tests" OFF)

# Configure JUCE
add_subdirectory(JUCE)

# Configure CLAP JUCE extension
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add the sources subdirectory (populates ARPS_CORE_SOURCES in parent scope)
add_subdirectory(src)

if(ARPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
