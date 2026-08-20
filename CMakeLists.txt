# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.22)

# Set the project name
project(ArpsEuclidya VERSION 0.5.0)

option(ARPS_BUILD_TESTS "Build unit tests" OFF)

include(cmake/JuceSource.cmake)
add_subdirectory("${CHALKWALK_JUCE_ROOT}" "${CMAKE_BINARY_DIR}/juce-build")

# Configure CLAP JUCE extension
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add the sources subdirectory (populates ARPS_CORE_SOURCES in parent scope)
# ---------------------------------------------------------------------------
# chalkwalk-music -- shared, JUCE-free music theory.
# Submodule: https://github.com/chalkwalk/chalkwalk-music (MIT).
#
# This project's Euclidean generator came from here originally and has now gone
# back, with its phase corrected -- see the note in tests/TestEuclideanDivergence.cpp.
# ---------------------------------------------------------------------------
set(CHALKWALK_MUSIC_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(libs/music)

add_subdirectory(src)

if(ARPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
