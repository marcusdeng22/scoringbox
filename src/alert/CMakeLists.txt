set(PROGNAME alert)

add_library(${PROGNAME} "")

pico_generate_pio_header(${PROGNAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_sources(${PROGNAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/alert.c
        ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/alert.h
)

target_include_directories(${PROGNAME}
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROGNAME}
    pico_stdlib
    hardware_pio
)