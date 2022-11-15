cmake_minimum_required(VERSION 3.1)
project(ds1963s-utils)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99")

add_subdirectory(src)
