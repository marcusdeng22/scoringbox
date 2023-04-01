#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
// #include "hardware/irq.h"
#include <time.h>
#include <spi.h>
#include <alert.h>

//CONSTANTS
#define NUMSOCKET   6
//access to socket info
//sockets: A B  C
#define LEFTA       0
#define LEFTB       1
#define LEFTC       2
#define RIGHTA      3
#define RIGHTB      4
#define RIGHTC      5

#define BUZZTIME    750    //in ms
#define LIGHTTIME   750    //in ms, will be turned off after BUZZTIME completes

#define MODEPIN     0
#define FMODEPIN    6
#define EMODEPIN    7
#define SMODEPIN    8

//COLOR DEF in RGB                RED       GREEN       BLUE
const uint32_t WHITE     = 255 << 8 | 255 << 16 | 255;
const uint32_t YELLOW    = 255 << 8 | 255 << 16 | 0;
const uint32_t RED       = 255 << 8 | 0 << 16   | 0;
const uint32_t GREEN     = 0 << 8   | 255 << 16 | 0;

//mode
const uint8_t FOIL_MODE  = 0;
const uint8_t EPEE_MODE  = 1;
const uint8_t SABRE_MODE = 2;
const uint8_t MODE_LIMIT = 2;

//state values
const uint16_t LOW  = 110;
const uint16_t MED  = 350;
const uint16_t HIGH = 900;

//GLOBALS
spi_inst_t * spi;
uint16_t sockets[NUMSOCKET];
volatile uint8_t mode = FOIL_MODE;

//in microseconds
const uint16_t DEPRESS_FOIL  = 14000;
// const uint32_t LOCKOUT_FOIL  = 300000;
const uint32_t LOCKOUT_FOIL = 1 * 1000 * 1000;

uint64_t timeLeft;
uint64_t timeRight;

//states
bool onLeft, offLeft, leftTouch, onRight, offRight, rightTouch;
bool lockout;
bool lightLeft, lightLeftWarn, lightRight, lightRightWarn, signalled;

static inline bool isLow(const uint16_t val) {
    return val < LOW;
}

static inline bool isMed(const uint16_t val) {
    return 450 < val && val < 620;
    // return 400 < val && val < 650;
    // return 430 < val && val < 650;
    // return 500 < val && val < 620;
    // return 520 < val && val < 560;
}

static inline bool isHigh(const uint16_t val) {
    return HIGH < val;
}

void toggleLight(uint pin) {
    gpio_put(pin, true);
    sleep_ms(1000);
    // gpio_put(pin, false);
}

void replaceLeft(uint32_t color) {
    printf("replace call %u\n", color);
    // return 0;
}

//this func will take color functions and push them to the LED strip since pushLED is slow
void core1Entry() {
    while (1) {
        //get function
        printf("multicore hello\n");
        multicore_fifo_pop_blocking();  //block until there is data, signalling we need to push the buffer
        pushLED();
        // toggleLight(26);
        // gpio_put(26, false);
        // void (*fp)(uint32_t) = (void(*)(uint32_t)) multicore_fifo_pop_blocking();

        // void (*fp)() = (void(*)()) multicore_fifo_pop_blocking();
        // (*fp)();

        // toggleLight(22);
        // if (fp == pushLED || fp == buzzOn || fp == clearLED || fp == buzzOff) {
        // if (false) {
        //     printf("multicore select alert\n");
        //     // (*fp)();
        // }
        // else {
        //     printf("multicore select func\n");
        //     uint32_t color = multicore_fifo_pop_blocking();
        //     // toggleLight(26); 
        //     // printf("multicore got color %u\n", color);
        //     // if (fp == putLeft) {
        //     //     printf("is replace left\n");
        //     // }
        //     // else {
        //     //     printf("unknown fp %p %p\n", fp, *fp);
        //     //     printf("replace left pointer %p %p\n", replaceLeft, &replaceLeft);
        //     // }
        //     (*fp)(color);
        //     // gpio_put(22, false);
        //     printf("multicore called func\n");
        //     pushLED();
        // }
    }
}

// static inline void multiPutColorF(void (*f)(uint32_t), uint32_t color) {
static inline void multiPutColorF() {
    // printf("multicore push 1\n");
    // multicore_fifo_push_blocking((uintptr_t) f);
    // multicore_fifo_push_blocking((uintptr_t) &pushLED);
    multicore_fifo_push_blocking(0);
    // printf("multicore push 2\n");
    // multicore_fifo_push_blocking(color);
}

void setModeLED() {
    if (mode == FOIL_MODE) {
        gpio_put(FMODEPIN, true);
        gpio_put(EMODEPIN, false);
        gpio_put(SMODEPIN, false);
    }
    else if (mode == EPEE_MODE) {
        gpio_put(FMODEPIN, true);
        gpio_put(EMODEPIN, true);
        gpio_put(SMODEPIN, false);
    }
    else if (mode == SABRE_MODE) {
        gpio_put(FMODEPIN, false);
        gpio_put(EMODEPIN, true);
        gpio_put(SMODEPIN, true);
    }
}

void changeMode() {
    mode ++;
    if (mode > MODE_LIMIT) {
        mode = FOIL_MODE;
    }
    setModeLED();
}

void resetValues() {
    lockout = false;
    onLeft = offLeft = leftTouch = onRight = offRight = rightTouch = false;
    timeLeft = timeRight = 0;
    lightLeft = lightLeftWarn = lightRight = lightRightWarn = signalled = false;
    sleep_ms(BUZZTIME);
    buzzOff();
    sleep_ms(LIGHTTIME);
    clearLED();
    printf("done resetting\n");
}

void signalHits() {
    if (lockout) {
        //a touch is long enough to block further inputs
        printf("lockout finished\n");
        resetValues();
    }
    //set the proper lights on a hit and buzz
    if (onLeft && !lightLeft) {
        putLeft(RED);
        // buzzOn();
        lightLeft = true;
        printf("signal left on\n");
    }
    if (offLeft && !lightLeft) {
        putLeft(WHITE);
        // buzzOn();
        lightLeft = true;
        printf("signal left off\n");
    }
    if (onRight && !lightRight) {
        putRight(GREEN);
        // buzzOn();
        lightRight = true;
        printf("signal right on\n");
    }
    if (offRight && !lightRight) {
        putRight(WHITE);
        // buzzOn();
        lightRight = true;
        printf("signal right off\n");
    }

    //group each led output together because it's slow; buzz is fast
    //todo: make this multicore because pushLED is inherently slow
    if (lightLeft || lightRight) {
        buzzOn();
        pushLED();
    }
    //warn for foil on self contact
}

void setup() {
    spi = initSPI();
    alertInit();
    // //temp indicator
    // gpio_init(22);
    // gpio_set_dir(22, GPIO_OUT);
    // gpio_init(26);
    // gpio_set_dir(26, GPIO_OUT);
    multicore_launch_core1(core1Entry);
    //setup interrupt for mode change
    gpio_set_irq_enabled_with_callback(MODEPIN, GPIO_IRQ_EDGE_RISE, true, &changeMode);
    //init mode LED pins
    gpio_init(FMODEPIN);
    gpio_set_dir(FMODEPIN, GPIO_OUT);
    gpio_init(EMODEPIN);
    gpio_set_dir(EMODEPIN, GPIO_OUT);
    gpio_init(SMODEPIN);
    gpio_set_dir(SMODEPIN, GPIO_OUT);
    setModeLED(FOIL_MODE);
    //test alert init
    sleep_ms(5000);
    // multiPutColorF(putLeft, RED);
    // sleep_ms(300);
    // multiPutColorF(putLeftWarn, YELLOW);
    // multiPutColorF(putRight, WHITE);
    // multiPutColorF(putRightWarn, YELLOW);

    putLeft(RED);
    putLeftWarn(YELLOW);
    putRight(GREEN);
    putRightWarn(YELLOW);
    multiPutColorF();
    // pushLED();
    buzzOn();
    sleep_ms(BUZZTIME);
    buzzOff();
    sleep_ms(LIGHTTIME);
    // clearLED();
    printf("setup done\n");
}

uint64_t prev;
void foil() {
    //process socket info for foil
    printf("foil: %f\n", (double) (time_us_64() - prev));
    for (uint8_t i = 0; i < NUMSOCKET; i++) {
        printf("%u ", sockets[i]);
    }
    printf("\n");

    uint64_t now = time_us_64();
    if (((onLeft || offLeft) && (timeLeft + LOCKOUT_FOIL < now)) ||
        ((onRight || offRight) && (timeRight + LOCKOUT_FOIL < now))) {
            lockout = true;
    }

    //left fencer (A)
    if (!onLeft && !offLeft) {  //ignore if left has hit something (already classified)
        if(isHigh(sockets[LEFTB]) && isLow(sockets[RIGHTA])) {
            //off target left
            if (!leftTouch) {   //start of hit
                timeLeft = time_us_64();
                leftTouch = true;
                // printf("left off start\n");
            }
            else {
                if (timeLeft + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    offLeft = true;
                    // printf("left off end\n");
                }
            }
        }
        else if (isMed(sockets[LEFTB]) && isMed(sockets[RIGHTA])) {
            //on left
            if (!leftTouch) {   //start of hit
                timeLeft = time_us_64();
                leftTouch = true;
                // printf("left on start\n");
            }
            else {
                if (timeLeft + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    onLeft = true;
                    // printf("left on end\n");
                }
            }
        }
        else {
            //too short of a hit: reset
            timeLeft = 0;
            leftTouch = false;
        }
    }

    //right fencer (B)
    if (!onRight && !offRight) {  //ignore if right has hit something (already classified)
        if(isHigh(sockets[RIGHTB]) && isLow(sockets[LEFTA])) {
            //off target right
            if (!rightTouch) {   //start of hit
                timeRight = time_us_64();
                rightTouch = true;
                // printf("right off start\n");
            }
            else {
                if (timeRight + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    offRight = true;
                    // printf("right off end\n");
                }
            }
        }
        else if (isMed(sockets[RIGHTB]) && isMed(sockets[LEFTA])) {
            //on right
            if (!rightTouch) {   //start of hit
                timeRight = time_us_64();
                rightTouch = true;
                // printf("right on start\n");
            }
            else {
                if (timeRight + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    onRight = true;
                    // printf("right on end\n");
                }
            }
        }
        else {
            //too short of a hit: reset
            timeRight = 0;
            rightTouch = false;
        }
    }

    prev = time_us_64();
}

void epee() {
    //process socket info for epee
    printf("epee\n");
}

void sabre() {
    //process socket info for sabre
    printf("sabre\n");
}

void loop() {
    readADC(spi, sockets, 0, NUMSOCKET);
    if (mode == FOIL_MODE) {
        foil();
    }
    else if (mode == EPEE_MODE) {
        epee();
    }
    else if (mode == SABRE_MODE) {
        sabre();
    }
    signalHits();
    sleep_ms(500);
}

int main() {
    stdio_init_all();
    //temp indicator
    // gpio_init(22);
    // gpio_set_dir(22, GPIO_OUT);
    // gpio_init(26);
    // gpio_set_dir(26, GPIO_OUT);
    // multicore_launch_core1(core1Entry);
    setup();
    while (true) {
        // loop();
    }
    return 0;
}