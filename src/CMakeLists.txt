cmake_minimum_required(VERSION 2.6)
project(ds1963s-utils)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=gnu99")

add_subdirectory(ibutton)

set(SOURCES ds1963s-client.c ds1963s-device.c ds1963s-error.c transport.c
            transport-factory.c transport-unix.c)

add_library(ds1963s ${SOURCES})
target_link_libraries(ds1963s ibutton)

add_executable(ds1963s-tool ds1963s-tool.c)
target_link_libraries(ds1963s-tool ds1963s crypto)

add_executable(ds1963s-emulator ds1963s-emulator.c ds2480-device.c 1-wire-bus.c)
target_link_libraries(ds1963s-emulator ds1963s)
