#
# Copyright 2019 Daniel Jones
#

cmake_minimum_required(VERSION 3.10)
project(freelist)

set(CMAKE_CXX_STANDARD 11)

add_library(freelist INTERFACE)
target_include_directories(freelist INTERFACE include)
target_link_libraries(freelist INTERFACE atomic)

add_subdirectory(tests)
