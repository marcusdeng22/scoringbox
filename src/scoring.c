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

#define BUZZTIME    1000    //in ms
#define LIGHTTIME   1000    //in ms, will be turned off after BUZZTIME completes

#define MODEPIN     0
#define FMODEPIN    6
#define EMODEPIN    7
#define SMODEPIN    8

//COLOR DEF in RGB         RED       GREEN       BLUE
const uint32_t WHITE     = 255 << 8 | 255 << 16 | 255;
const uint32_t YELLOW    = 255 << 8 | 255 << 16 | 0;
const uint32_t RED       = 255 << 8 | 0 << 16   | 0;
const uint32_t GREEN     = 0 << 8   | 255 << 16 | 0;
const uint32_t OFF       = 0;

//mode
const uint8_t FOIL_MODE  = 0;
const uint8_t EPEE_MODE  = 1;
const uint8_t SABRE_MODE = 2;
const uint8_t MODE_LIMIT = 2;

//state values
const uint16_t LOW  = 110;
const uint16_t MED1 = 450;
const uint16_t MED2 = 620;
const uint16_t HIGH = 900;

//GLOBALS
spi_inst_t * spi;
uint16_t sockets[NUMSOCKET];
volatile uint8_t mode = FOIL_MODE;

//in microseconds
const uint16_t DEPRESS_FOIL  = 14000;
// const uint32_t LOCKOUT_FOIL  = 300000;
const uint32_t LOCKOUT_FOIL = 2 * 1000 * 1000;

uint64_t timeLeft;
uint64_t timeRight;

//states
bool onLeft, offLeft, warnLeft, leftTouch, onRight, offRight, warnRight, rightTouch;
bool lockout;
bool lightLeft, lightLeftWarn, lightRight, lightRightWarn;

static inline bool isLow(const uint16_t val) {
    return val < LOW;
    // return val < 200;
}

static inline bool isMed(const uint16_t val) {
    return MED1 < val && val < MED2;
    // return 400 < val && val < 650;
    // return 430 < val && val < 650;
    // return 500 < val && val < 620;
    // return 520 < val && val < 560;
}

static inline bool isHigh(const uint16_t val) {
    return HIGH < val;
}

static inline bool isWarn(const uint16_t val) {
    return LOW < val && val < MED1;
}

//this func will take color functions and push them to the LED strip since pushLED is slow
void core1Entry() {
    while (1) {
        // multicore_fifo_pop_blocking();  //block until there is data, signalling we need to push the buffer
        // pushLED();
        void (*f)() = (void (*)()) multicore_fifo_pop_blocking();
        if (f == pushLED) {
            pushLED();
        }
        else if (f == clearLED) {
            sleep_ms(LIGHTTIME);
            clearLED();
            //notify back to core 0
            multicore_fifo_push_blocking(0);
        }
        else {
            uint32_t color = multicore_fifo_pop_blocking();
            (*f)(color);
            pushLED();
            sleep_ms(1);
        }
    }
}

static inline void multicorePut(void (*f)(uint32_t), uint32_t color) {
    multicore_fifo_push_blocking((uintptr_t) f);
    multicore_fifo_push_blocking(color);
}

static inline void multicorePushLED() {
    multicore_fifo_push_blocking((uintptr_t) &pushLED);
}

//if want to set a warn light, use OFF color with pushL/RWarn and multicorePushLED()
static inline void multicoreClearLED() {
    multicore_fifo_push_blocking((uintptr_t) &clearLED);
    //wait until the clear is done
    multicore_fifo_pop_blocking();
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
    // uint64_t resetStart = time_us_64();
    // uint64_t resetEnd;
    lockout = false;
    onLeft = offLeft = warnLeft = leftTouch = onRight = offRight = warnRight = rightTouch = false;
    timeLeft = timeRight = 0;
    lightLeft = lightLeftWarn = lightRight = lightRightWarn = false;
    // resetEnd = time_us_64();
    // printf("resetting vars: %f\n", (double) (resetEnd - resetStart));

    //this should be a little more than BUZZTIME (1000)
    // resetStart = time_us_64();
    sleep_ms(BUZZTIME);
    buzzOff();
    // resetEnd = time_us_64();
    // printf("resetting buzz: %f\n", (double) (resetEnd - resetStart));

    //this should be a little more than LIGHTTIME (1000) + time to write since we block (approx 1000 us)
    // resetStart = time_us_64();
    multicoreClearLED();
    // resetEnd = time_us_64();
    // printf("done resetting %f\n", (double) (resetEnd - resetStart));
}

void signalHits() {
    // uint64_t signalStart = time_us_64();
    // uint64_t end;
    if (lockout) {
        //a touch is long enough to block further inputs
        resetValues();
        // end = time_us_64();
        // printf("lockout finished %f\n", (double)(end - signalStart));
    }
    // uint64_t start = time_us_64();
    // printf("signal lockout check %f\n", (double) (start - signalStart));
    // end = time_us_64();
    // printf("time to print %f\n", (double) (end - start));

    //set the proper lights on a hit and buzz
    // start = time_us_64();
    if (onLeft && !lightLeft) {
    // if (!lightLeft && onLeft) {
        multicorePut(putLeft, RED);
        buzzOn();
        lightLeft = true;
        // end = time_us_64();
        // printf("signal left on %f\n", (double)(end - start));
    }
    // end = time_us_64();
    // printf("time to check left on %f\n", (double) (end - start));

    // start = time_us_64();
    if (offLeft && !lightLeft) {
    // if (!lightLeft && offLeft) {
        multicorePut(putLeft, WHITE);
        buzzOn();
        lightLeft = true;
        // end = time_us_64();
        // printf("signal left off %f\n", (double)(end - start));
    }
    // end = time_us_64();
    // printf("time to check left off %f\n", (double) (end - start));

    // start = time_us_64();
    if (onRight && !lightRight) {
    // if (!lightRight && onRight) {
        multicorePut(putRight, GREEN);
        buzzOn();
        lightRight = true;
        // end = time_us_64();
        // printf("signal right on %f\n", (double)(end - start));
    }
    // end = time_us_64();
    // printf("time to check right on %f\n", (double) (end - start));

    // start = time_us_64();
    if (offRight && !lightRight) {
    // if (!lightRight && offRight) {
        multicorePut(putRight, WHITE);
        buzzOn();
        lightRight = true;
        // end = time_us_64();
        // printf("signal right off %f\n", (double)(end - start));
    }
    // end = time_us_64();
    // printf("signal end %f\n", (double)(end - signalStart));

    if (warnLeft && !lightLeftWarn) {
        multicorePut(putLeftWarn, YELLOW);
        lightLeftWarn = true;
    }
    else if (!warnLeft && lightLeftWarn) {
        multicorePut(putLeftWarn, OFF);
        lightLeftWarn = false;
    }

    if (warnRight && !lightRightWarn) {
        multicorePut(putRightWarn, YELLOW);
        lightRightWarn = true;
    }
    else if (!warnRight && lightRightWarn) {
        multicorePut(putRightWarn, OFF);
        lightRightWarn = false;
    }

    //group each led output together because it's slow; buzz is fast
    //todo: make this multicore because pushLED is inherently slow
    // if (lightLeft || lightRight) {
    //     buzzOn();
    //     pushLED();
    // }
    //warn for foil on self contact
}

void setup() {
    spi = initSPI();
    alertInit();

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

    //init delay for testing
    // sleep_ms(7000);

    multicorePut(putLeft, RED);
    multicorePut(putLeftWarn, YELLOW);
    multicorePut(putRight, GREEN);
    multicorePut(putRightWarn, YELLOW);

    // multicorePushLED();
    buzzOn();
    sleep_ms(BUZZTIME);
    buzzOff();
    multicoreClearLED();
    printf("setup done\n");
}

uint64_t prev;
void foil() {
    //process socket info for foil
    uint64_t debugNow = time_us_64();
    //500 ms delay between each loop, 2s delay from lights
    printf("foil: %f\n", (double) (debugNow - prev));

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

    //left warn (self touch)
    // if (sockets[LEFTA] > 200 && sockets[LEFTA] < 420) {
    if (isWarn(sockets[LEFTA])) {
        warnLeft = true;
    }
    else {
        warnLeft = false;
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

    //right warn (self touch)
    // if (sockets[RIGHTA] > LOW && sockets[RIGHTA] < 420) {
    if (isWarn(sockets[RIGHTA])) {
        warnRight = true;
    }
    else {
        warnRight = false;
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
    setup();
    while (true) {
        loop();
    }
    return 0;
}