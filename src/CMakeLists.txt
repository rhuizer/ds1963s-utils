cmake_minimum_required(VERSION 2.6)
project(ds1963s-utils)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99")

add_subdirectory(ibutton)
add_executable(ds1963s-brute ds1963s-brute.c ds1963s.c ds1963s-device.c sha1.c)
target_link_libraries(ds1963s-brute ibutton)
add_executable(ds1963s-tool ds1963s-tool.c ds1963s.c)
target_link_libraries(ds1963s-tool ibutton)
