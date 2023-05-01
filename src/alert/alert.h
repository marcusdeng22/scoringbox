#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#define IS_RGBW false
#define NUM_PIXELS 60

/*
there are 6 LEDs per row
arrangement of LEDS:
         LEFT      RIGHT
warn  -> 0>++++ ------> 0>++++ -|
break |- ------ <- h    ------  |
r1    -> 0>++++ -| h |- ++++<0 <-
r2    |- ++++<0 <- h -> 0>++++ -|
r3    -> 0>++++ -|   |- ++++<0 <-
r4    |- ++++<0 <- s -> 0>++++

s is start, with wire going left
0<> indicates start and direction
6 for warning lights    x2
24 for on/off lights    x2
--------------------------
30 lights               60 total
*/

#define LEFT        0
#define LEFTWARN    24
#define RIGHTWARN   30
#define RIGHT       36

#define WS2812_PIN  21
#define BUZZER      22

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static uint32_t ledArr[NUM_PIXELS];

static inline void putLeftWarn(uint32_t color) {
    for (int i = LEFTWARN; i < RIGHTWARN; i++) {
        ledArr[i] = color;
    }
}

static inline void putRightWarn(uint32_t color) {
    for (int i = RIGHTWARN; i < RIGHT; i++) {
        ledArr[i] = color;
    }
}

static inline void putLeft(uint32_t color) {
    for (int i = LEFT; i < LEFTWARN; i++) {
        ledArr[i] = color;
    }
}

static inline void putRight(uint32_t color) {
    for (int i = RIGHT; i < NUM_PIXELS; i++) {
        ledArr[i] = color;
    }
}

static inline void pushLED() {
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(ledArr[i]);
    }
}

static inline void buzzOn() {
    gpio_put(BUZZER, true);
}

static inline void clearLED() {
    for (int i = 0; i < NUM_PIXELS; i++) {
        ledArr[i] = 0;
    }
    pushLED();
}

static inline void buzzOff() {
    gpio_put(BUZZER, false);
}

static inline void alertInit() {
    PIO pio = pio0;
    uint8_t sm = 0; //state machine id, 0-4
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
}