#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <time.h>

spi_inst_t * initSPI();

void readADC(spi_inst_t *spi, uint16_t sockets[], int channelStart, int channelEnd);