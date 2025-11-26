cmake_minimum_required(VERSION 3.18.1)

project("fastseekdecoder")

# Set C++ standard and properties
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
find_library(jnigraphics-lib jnigraphics)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp)

# Link libraries
target_link_libraries(
    fastseekdecoder
    ${log-lib}
    ${android-lib}
    ${mediandk-lib}
    ${jnigraphics-lib}
)

# Add compilation flags
target_compile_options(fastseekdecoder PRIVATE -Wall -Wextra -Werror)
