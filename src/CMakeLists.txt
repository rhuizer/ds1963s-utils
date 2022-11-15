FIND_LIBRARY(LIBYAML NAMES yaml libyaml)
if (LIBYAML)
    add_definitions(-DHAVE_LIBYAML)
endif()

add_subdirectory(ibutton)

set(SOURCES ds1963s-common.c ds1963s-client.c ds1963s-device.c ds1963s-error.c
            ds2480b-device.c transport.c transport-factory.c transport-unix.c
            transport-pty.c coroutine.c 1-wire-bus.c)
add_library(ds1963s ${SOURCES})
target_link_libraries(ds1963s ibutton)

if (LIBYAML)
add_executable(ds1963s-tool ds1963s-tool.c ds1963s-tool-yaml.c)
target_link_libraries(ds1963s-tool ds1963s crypto yaml)

add_executable(ds1963s-emulator ds1963s-emulator.c ds1963s-emulator-yaml.c)
target_link_libraries(ds1963s-emulator ds1963s crypto yaml)
else()
add_executable(ds1963s-tool ds1963s-tool.c ds1963s-tool-yaml.c)
target_link_libraries(ds1963s-tool ds1963s crypto)

add_executable(ds1963s-emulator ds1963s-emulator.c)
target_link_libraries(ds1963s-emulator ds1963s crypto)
endif()

add_executable(ds1963s-shell ds1963s-shell.c)
target_link_libraries(ds1963s-shell ds1963s readline)
