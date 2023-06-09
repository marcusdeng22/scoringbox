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
#define LEFTA       3
#define LEFTB       4
#define LEFTC       5
#define RIGHTA      0
#define RIGHTB      1
#define RIGHTC      2

#define BUZZTIME    1000    //in ms
#define LIGHTTIME   1000    //in ms, will be turned off after BUZZTIME completes

#define MODEPIN     0
#define FMODEPIN    6
#define EMODEPIN    7
#define SMODEPIN    8

#define DEBUG false

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
const uint8_t MODE_LIMIT = 1;

//state values
const uint16_t LOW  = 110;
const uint16_t MED1 = 400;
const uint16_t MED2 = 620;
const uint16_t HIGH = 900;

const uint16_t FOIL_WARN = 280;
const uint16_t FOIL_MED = 420;
const uint16_t SABR_LOW = 200;
const uint16_t SABR_WARN = 300;

//GLOBALS
spi_inst_t * spi;
uint16_t sockets[NUMSOCKET];
volatile uint8_t mode = FOIL_MODE;
volatile bool modeChange = false;

//in microseconds
const uint32_t DEPRESS_FOIL  = 14000;
const uint32_t LOCKOUT_FOIL  = 300000;
const uint32_t DEPRESS_EPEE  = 2000;
const uint32_t LOCKOUT_EPEE  = 45000;
const uint32_t DEPRESS_SABRE = 100;
const uint32_t LOCKOUT_SABRE = 170000;

uint64_t timeLeft;
uint64_t timeRight;

//states
bool onLeft, offLeft, warnLeft, leftTouch, onRight, offRight, warnRight, rightTouch;
bool lockout;
bool lightLeft, lightLeftWarn, lightRight, lightRightWarn;

static inline bool isLow(const uint16_t val) {
    return val <= LOW;
}

static inline bool isMed(const uint16_t val) {
    return MED1 < val && val < MED2;
}

static inline bool isHigh(const uint16_t val) {
    return HIGH < val;
}

static inline bool isFoilWarn(const uint16_t val) {
    return FOIL_WARN < val && val <= MED1;
}

static inline bool isFoilMed(const uint16_t val) {
    return FOIL_MED < val && val < MED2;
}

static inline bool isSabrMed(const uint16_t val) {
    // return SABR_WARN < val && val < MED2;
    return SABR_LOW < val && val < SABR_WARN;
}

//this func will take color functions and push them to the LED strip since pushLED is slow
void core1Entry() {
    while (1) {
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
            sleep_ms(1);    //delay so that the ws2812 protocol can propogate!
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

void resetValues() {
    lockout = false;
    onLeft = offLeft = warnLeft = leftTouch = onRight = offRight = warnRight = rightTouch = false;
    timeLeft = timeRight = 0;
    lightLeft = lightLeftWarn = lightRight = lightRightWarn = false;
    sleep_ms(BUZZTIME);
    buzzOff();
    multicoreClearLED();
}

//just a flag because we only want to trigger once
void changeMode() {
    modeChange = true;
}

void signalHits() {
    if (lockout) {
        //a touch is long enough to block further inputs
        resetValues();
    }

    //set the proper lights on a hit and buzz
    if (onLeft && !lightLeft) {
        multicorePut(putLeft, RED);
        buzzOn();
        lightLeft = true;
    }
    if (offLeft && !lightLeft) {
        multicorePut(putLeft, WHITE);
        buzzOn();
        lightLeft = true;
    }
    if (onRight && !lightRight) {
        multicorePut(putRight, GREEN);
        buzzOn();
        lightRight = true;
    }
    if (offRight && !lightRight) {
        multicorePut(putRight, WHITE);
        buzzOn();
        lightRight = true;
    }

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
}

void setup() {
    spi = initSPI();
    alertInit();

    multicore_launch_core1(core1Entry);
    //setup interrupt for mode change
    gpio_set_irq_enabled_with_callback(MODEPIN, GPIO_IRQ_EDGE_FALL, true, &changeMode);
    //init mode LED pins
    gpio_init(FMODEPIN);
    gpio_set_dir(FMODEPIN, GPIO_OUT);
    gpio_init(EMODEPIN);
    gpio_set_dir(EMODEPIN, GPIO_OUT);
    gpio_init(SMODEPIN);
    gpio_set_dir(SMODEPIN, GPIO_OUT);
    setModeLED();

    multicorePut(putLeft, RED);
    multicorePut(putLeftWarn, YELLOW);
    multicorePut(putRight, GREEN);
    multicorePut(putRightWarn, YELLOW);

    buzzOn();
    sleep_ms(BUZZTIME);
    buzzOff();
    multicoreClearLED();
}

#ifdef DEBUG
uint64_t prev;
#endif
void foil() {
    //process socket info for foil
    uint64_t debugNow = time_us_64();
    //500 ms delay between each loop, 2s delay from lights
#ifdef DEBUG
    printf("foil: %f\n", (double) (debugNow - prev));   //approx 100 us per loop

    for (uint8_t i = 0; i < NUMSOCKET; i++) {
        printf("%u ", sockets[i]);
    }
    printf("\n");
#endif

    uint64_t now = time_us_64();
    if (((onLeft || offLeft) && (timeLeft + LOCKOUT_FOIL < now)) || ((onRight || offRight) && (timeRight + LOCKOUT_FOIL < now))) {
            lockout = true;
    }

    //left fencer (A)
    if (!onLeft && !offLeft) {  //ignore if left has hit something (already classified)
        if(isHigh(sockets[LEFTB]) && sockets[RIGHTA] < MED1) { //(isLow(sockets[RIGHTA]) && isFoilWarn(sockets[RIGHTA]))
            //off target left
            if (!leftTouch) {   //start of hit
                timeLeft = time_us_64();
                leftTouch = true;
            }
            else {
                if (timeLeft + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    offLeft = true;
                }
            }
        }
        else if (isFoilMed(sockets[LEFTB]) && isFoilMed(sockets[RIGHTA])) {
            //on left
            if (!leftTouch) {   //start of hit
                timeLeft = time_us_64();
                leftTouch = true;
            }
            else {
                if (timeLeft + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    onLeft = true;
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
    if (isFoilWarn(sockets[LEFTA])) {
        warnLeft = true;
    }
    else {
        warnLeft = false;
    }

    //right fencer (B)
    if (!onRight && !offRight) {  //ignore if right has hit something (already classified)
        if(isHigh(sockets[RIGHTB]) && sockets[LEFTA] < MED1) { //(isLow(sockets[LEFTA])) || isFoilWarn(sockets[LEFTA]))
            //off target right
            if (!rightTouch) {   //start of hit
                timeRight = time_us_64();
                rightTouch = true;
            }
            else {
                if (timeRight + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    offRight = true;
                }
            }
        }
        else if (isFoilMed(sockets[RIGHTB]) && isFoilMed(sockets[LEFTA])) {
            //on right
            if (!rightTouch) {   //start of hit
                timeRight = time_us_64();
                rightTouch = true;
            }
            else {
                if (timeRight + DEPRESS_FOIL <= time_us_64()) {
                    //long enough hit
                    onRight = true;
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
    if (isFoilWarn(sockets[RIGHTA])) {
        warnRight = true;
    }
    else {
        warnRight = false;
    }

#ifdef DEBUG
    prev = time_us_64();
#endif
}

void epee() {
    //process socket info for epee
    uint64_t debugNow = time_us_64();
    //500 ms delay between each loop, 2s delay from lights
#ifdef DEBUG
    printf("epee: %f\n", (double) (debugNow - prev));   //approx ??? us per loop

    for (uint8_t i = 0; i < NUMSOCKET; i++) {
        printf("%u ", sockets[i]);
    }
    printf("\n");
#endif

    uint64_t now = time_us_64();
    if ((onLeft && (timeLeft + LOCKOUT_EPEE < now)) || (onRight && (timeRight + LOCKOUT_EPEE < now))) {
            lockout = true;
    }

    //left fencer (A)
    if (!onLeft) {  //ignore if left has hit something (already classified)
        if (isMed(sockets[LEFTB]) && isMed(sockets[LEFTA])) {
            //B weapon pin and A pin completed
            if (!leftTouch) {   //start of hit
                timeLeft = time_us_64();
                leftTouch = true;
            }
            else {
                if (timeLeft + DEPRESS_EPEE <= time_us_64()) {
                    //long enough hit
                    onLeft = true;
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
    if (!onRight) {  //ignore if right has hit something (already classified)
        if (isMed(sockets[RIGHTB]) && isMed(sockets[RIGHTA])) {
            //B weapon pin and A pin completed
            if (!rightTouch) {   //start of hit
                timeRight = time_us_64();
                rightTouch = true;
            }
            else {
                if (timeRight + DEPRESS_EPEE <= time_us_64()) {
                    //long enough hit
                    onRight = true;
                }
            }
        }
        else {
            //too short of a hit: reset
            timeRight = 0;
            rightTouch = false;
        }
    }

#ifdef DEBUG
    prev = time_us_64();
#endif
}

//this is disabled in code with MODE_LIMIT
void sabre() {
    //process socket info for sabre
    uint64_t debugNow = time_us_64();
    //500 ms delay between each loop, 2s delay from lights
#ifdef DEBUG
    printf("sabre: %f\n", (double) (debugNow - prev));   //approx ??? us per loop

    for (uint8_t i = 0; i < NUMSOCKET; i++) {
        printf("%u ", sockets[i]);
    }
    printf("\n");
#endif

    uint64_t now = time_us_64();
    if ((onLeft && (timeLeft + LOCKOUT_SABRE < now)) || (onRight && (timeRight + LOCKOUT_SABRE < now))) {
            lockout = true;
    }

    //left fencer (A)
    if (!onLeft) {  //ignore if left has hit something (already classified)
        if (isSabrMed(sockets[LEFTB]) && isSabrMed(sockets[RIGHTA])) {
            //B weapon pin and right A lame pin completed
            if (!leftTouch) {
                timeLeft = time_us_64();
                leftTouch = true;
            }
            else {
                if (timeLeft + DEPRESS_SABRE <= time_us_64()) {
                    //long enough hit
                    onLeft = true;
                }
            }
        }
        else {
            //too short of a hit; reset
            timeLeft = 0;
            leftTouch = false;
        }
    }

    //left warn (self touch)
    if (isSabrMed(sockets[LEFTA])) {
        warnLeft = true;
    }
    else {
        warnLeft = false;
    }

    //right fencer (B)
    if (!onRight) {  //ignore if left has hit something (already classified)
        if (isSabrMed(sockets[RIGHTB]) && isSabrMed(sockets[LEFTA])) {
            //B weapon pin and right A lame pin completed
            if (!rightTouch) {
                timeRight = time_us_64();
                rightTouch = true;
            }
            else {
                if (timeRight + DEPRESS_SABRE <= time_us_64()) {
                    //long enough hit
                    onRight = true;
                }
            }
        }
        else {
            //too short of a hit; reset
            timeRight = 0;
            rightTouch = false;
        }
    }

    //right warn (self touch)
    if (isSabrMed(sockets[RIGHTA])) {
        warnRight = true;
    }
    else {
        warnRight = false;
    }

#ifdef DEBUG
    prev = time_us_64();
#endif
}

void checkMode() {
    if (modeChange) {
        mode ++;
        if (mode > MODE_LIMIT) {
            mode = FOIL_MODE;
        }
        setModeLED();
        resetValues();
        modeChange = false;
    }
}

void loop() {
    checkMode();
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
    // sleep_ms(500);
}

int main() {
    stdio_init_all();
    setup();
    while (true) {
        loop();
    }
    return 0;
}