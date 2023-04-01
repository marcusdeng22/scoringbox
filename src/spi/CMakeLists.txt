set(PROGNAME spi)

add_library(${PROGNAME} "")
# add_library(${PROGNAME} STATIC spi.h)

target_sources(${PROGNAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/spi.c
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/spi.h
)

target_include_directories(${PROGNAME}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROGNAME}
    pico_stdlib
    hardware_spi
)