#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <time.h>

static const size_t BUF_SIZE = 3;

//pin def
static const uint8_t chipselect = 17;
static const uint8_t mosi = 19;
static const uint8_t miso = 16;
static const uint8_t clck = 18;

static uint16_t readChannel(spi_inst_t *spi, const uint8_t cs, const uint8_t channel) {
    uint8_t command = 0b11 << 6;
    command |= (channel & 0x07) << 3;

    uint8_t data[BUF_SIZE];
    data[0] = command;

    uint8_t result[BUF_SIZE];
    // uint8_t * const result[BUF_SIZE];

    //set cs to 0 to start comm
    gpio_put(cs, 0);

    //begin read/write
    spi_write_read_blocking(spi, data, result, BUF_SIZE);

    //set cs to 1 to end comm
    gpio_put(cs, 1);

    //convert results
    uint16_t ret = (result[0] & 0x01) << 9;
    ret |= (result[1] & 0xFF) << 1;
    ret |= (result[2] & 0x80) >> 7;
    return ret;
}

spi_inst_t * initSPI() {
    //get spi 0
    spi_inst_t *spi = spi0;
    //initialize spi at 2MHz, maybe 3?
    spi_init(spi, 2000 * 1000);
    //set spi format
    spi_set_format(spi,
        8,  //# of bits
        0,  //polarity
        0,  //phase
        SPI_MSB_FIRST   //order
    );
    //initialize spi pins
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);
    gpio_set_function(clck, GPIO_FUNC_SPI);

    //initialize chip select to high, active-low
    gpio_init(chipselect);
    gpio_set_dir(chipselect, GPIO_OUT);
    gpio_put(chipselect, 1);

    return spi;
}

void readADC(spi_inst_t *spi, uint16_t sockets[], int channelStart, int channelEnd) {
    // clock_t start = (clock_t) time_us_64();
    for (uint8_t i = channelStart; i < channelEnd; i++){
        sockets[i] = readChannel(spi, chipselect, i);
    }
    // clock_t end = (clock_t) time_us_64();
    // for (size_t i = 0; i < 6; i++) {
    //     printf("%u ", sockets[i]);
    // }
    // printf("\n");
    // printf("total execution time: %f\n\n", (double)(end - start));
}