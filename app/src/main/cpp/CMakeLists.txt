cmake_minimum_required(VERSION 3.18.1)

# Define the library name
project("fastseekdecoder")

# Create the library
add_library(
    fastseekdecoder
    SHARED
    fast_seek_decoder.cpp
)

# Find required libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(mediandk-lib mediandk)

# Include directories
include_directories(src/main/cpp)

# Link libraries
target_link_libraries(
    fastseekdecoder
    ${log-lib}
    ${android-lib}
    ${mediandk-lib}
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
