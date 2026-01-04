# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.22)

# Set the project name
project(EuclideanArp VERSION 0.1.0)

# Configure JUCE
add_subdirectory(JUCE)

# Add the sources subdirectory
add_subdirectory(src)
