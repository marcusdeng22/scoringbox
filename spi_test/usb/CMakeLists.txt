if (TARGET tinyusb_device)
    set(PROGNAME spi_test)
    add_executable(${PROGNAME}
            spi_test.c
            )

    # pull in common dependencies
    target_link_libraries(${PROGNAME} pico_stdlib hardware_spi)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(${PROGNAME} 1)
    pico_enable_stdio_uart(${PROGNAME} 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(${PROGNAME})
elseif(PICO_ON_DEVICE)
    message(WARNING "not building spi_test because TinyUSB submodule is not initialized in the SDK")
endif()
