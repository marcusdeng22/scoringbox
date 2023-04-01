if (TARGET tinyusb_device)
    set(PROGNAME led_test)
    add_executable(${PROGNAME}
            led_test.c
            )

    pico_generate_pio_header(${PROGNAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

    # pull in common dependencies
    target_link_libraries(${PROGNAME} pico_stdlib hardware_pio)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(${PROGNAME} 1)
    pico_enable_stdio_uart(${PROGNAME} 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(${PROGNAME})
elseif(PICO_ON_DEVICE)
    message(WARNING "not building spi_test because TinyUSB submodule is not initialized in the SDK")
endif()
