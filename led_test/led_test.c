#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"
#include <time.h>

#define IS_RGBW false
#define NUM_PIXELS 30

#define WS2812_PIN 21

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) |
         ((uint32_t)(g) << 16) |
         (uint32_t)(b);
}

void pattern_greys(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

void pattern_red(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(urgb_u32(255, 0, 0));
        if (++t >= max) t = 0;
    }
}

void color_arr(uint32_t color, uint len, uint start) {
    clock_t starttime = (clock_t) time_us_64();
    for (int i = start; i < len; ++i) {
        put_pixel(color);
    }
    clock_t endtime = (clock_t) time_us_64();
    printf("time to push: %f\n\n", (double)(endtime - starttime));
}

int main() {
    stdio_init_all();

    PIO pio = pio0;
    uint8_t sm = 0; //state machine id, 0-4
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // put_pixel(urgb_u32(255, 0, 0));
    // sleep_ms(1000);
    // put_pixel(urgb_u32(0, 255, 0));
    // sleep_ms(1000);
    // put_pixel(urgb_u32(0, 0, 255));
    // sleep_ms(1000);
    // put_pixel(urgb_u32(0, 0, 0));
    // sleep_ms(1000);
    // put_pixel(urgb_u32(255, 255, 255));

    color_arr(urgb_u32(255, 0, 0), NUM_PIXELS, 0);
    sleep_ms(1000);
    color_arr(urgb_u32(0, 255, 0), NUM_PIXELS, 0);
    sleep_ms(1000);
    color_arr(urgb_u32(0, 0, 255), NUM_PIXELS, 0);
    sleep_ms(1000);
    color_arr(urgb_u32(0, 0, 0), NUM_PIXELS, 0);
    sleep_ms(1000);
    color_arr(urgb_u32(255, 255, 0), NUM_PIXELS, 0);
    sleep_ms(1000);
    color_arr(urgb_u32(255, 255, 255), NUM_PIXELS, 0);
    // sleep_ms(1000);

    // int t = 0;
    // while (1) {
    //     if (t < 5) {
    //         pattern_red(NUM_PIXELS, 0);
    //         sleep_ms(1000);
    //         t++;
    //     }
    //     else {
    //         for (int i = 0; i < NUM_PIXELS; ++i) {
    //             put_pixel(0);
    //         }
    //         break;
    //     }
    // }

    // int t = 0;
    // while (1) {
    //     pattern_greys(NUM_PIXELS, t);
    //     sleep_ms(10);
    //     t+= 1;
    //     if (t % 100 == 0) {
    //         printf(". ");
    //     }
    // }
    return 0;
}