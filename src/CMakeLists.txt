if (TARGET tinyusb_device)
    set(PROGNAME scoringbox)
    set(HEADER_FILES spi.h)

    add_executable(${PROGNAME}
        scoring.c
    )

    add_subdirectory(spi)
    add_subdirectory(alert)

    # pull in common dependencies
    target_link_libraries(${PROGNAME}
        PRIVATE
            pico_stdlib
            pico_multicore
            hardware_spi
            spi
            alert
    )

    # enable usb output, disable uart output
    pico_enable_stdio_usb(${PROGNAME} 1)
    pico_enable_stdio_uart(${PROGNAME} 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(${PROGNAME})
elseif(PICO_ON_DEVICE)
    message(WARNING "not building spi_test because TinyUSB submodule is not initialized in the SDK")
endif()
