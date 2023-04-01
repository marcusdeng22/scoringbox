#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"

#define BUTTON_PIN 0

#define SPEAKER1 15

int main() {
    stdio_init_all();
    gpio_init(BUTTON_PIN);
    gpio_init(SPEAKER1);

    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_down(BUTTON_PIN);
    gpio_set_dir(SPEAKER1, GPIO_OUT);

    while (1) {
        if (gpio_get(BUTTON_PIN)) {
            printf("button pushed!\n");
            clock_t start = (clock_t) time_us_64();
            gpio_put(SPEAKER1, true);
            clock_t end = (clock_t) time_us_64();
            printf("time to raise buzzer: %f\n", (double)(start - end));
            sleep_ms(1000);
            start = (clock_t) time_us_64();
            gpio_put(SPEAKER1, false);
            end = (clock_t) time_us_64();
            printf("time to stop buzzer: %f\n", (double)(start - end));
        }
        else{
            // printf("button up\n");
            // gpio_put(SPEAKER1, false);
            // sleep_ms(1000);
        }
    }

    return 0;
}