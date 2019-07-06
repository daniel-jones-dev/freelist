cmake_minimum_required(VERSION 3.10)
project(freelist)

set(CMAKE_CXX_STANDARD 11)

add_library(freelist INTERFACE)
#target_sources(freelist INTERFACE include/freelist/freelist.h)
target_include_directories(freelist INTERFACE include)

add_subdirectory(tests)
