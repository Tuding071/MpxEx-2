cmake_minimum_required(VERSION 3.18.1)

project("fastseekdecoder")

add_library(
    fastseekdecoder
    SHARED
    fast_seek_decoder.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(mediandk-lib mediandk)

include_directories(src/main/cpp)

target_link_libraries(
    fastseekdecoder
    ${log-lib}
    ${android-lib}
    ${mediandk-lib}
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
