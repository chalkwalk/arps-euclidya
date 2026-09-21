# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.22)

# Set the project name
project(ArpsEuclidya VERSION 0.5.0)

option(ARPS_BUILD_TESTS "Build unit tests" OFF)

# C++20 -- applies to every target in the tree, including the format-specific
# plugin wrappers that juce_add_plugin creates. A per-target PUBLIC cxx_std_20
# does NOT reach those wrappers, because JUCE compiles their sources
# independently with its own cxx_std_17 floor (see JUCEUtils.cmake).
#
# It is a single standard across the build rather than a preference: JUCE has
# inlines gated on __cpp_char8_t, so a tree that compiles some translation
# units at 17 and some at 20 is an ODR hazard, not merely an inconsistent one.
# Same reasoning and same lines as Anvil, Antiphon and the sequencer.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

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
# Added through the shared helper, like every other project in the ecosystem.
# That is what makes CHALKWALK_MUSIC_DIR work -- a working checkout can be
# tested HERE without a commit and without a push, which matters most when the
# library is the thing under test. Configure prints OVERRIDE when one is in
# use, because the submodule SHA then no longer describes the build.
include(cmake/ChalkwalkLibrary.cmake)
chalkwalk_add_library(music libs/music)

# ---------------------------------------------------------------------------
# chalkwalk-seed -- deterministic derivation.
# Submodule: https://github.com/chalkwalk/chalkwalk-seed (MIT, JUCE-free).
#
# Here to fix a real bug rather than to tidy anything. `WalkNode` seeds a
# generator from the sorted steps "so it's stable", and then draws through
# `std::uniform_real_distribution` -- which the standard deliberately leaves
# unspecified, so libstdc++ and libc++ return different numbers from the same
# engine state. The seed was a `size_t`, which is four bytes on some platforms
# and eight on others, so it differed before the engine even started.
#
# The result: the same patch walks differently on macOS than on Linux, and it
# fails silently, because each platform is perfectly consistent with itself.
# ---------------------------------------------------------------------------
chalkwalk_add_library(seed libs/seed)

add_subdirectory(src)

if(ARPS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
