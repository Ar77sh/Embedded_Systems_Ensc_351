/* 
 * My reaction timer game for the BeagleY-AI board.
 * Uses LEDs and joystick — pretty much what the assignment asks for.
 * Everything below runs on Linux (Debian ARM) using HAL drivers I wrote earlier.
 */

#define _POSIX_C_SOURCE 200809L   // needed for nanosleep() and clock_gettime()

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "hal/led.h"
#include "hal/joystick.h"

// quick helper to pause for a few ms without using usleep() directly
static void sleep_ms(int ms)
{
    struct timespec req;
    req.tv_sec  = ms / 1000;               // seconds part
    req.tv_nsec = (ms % 1000) * 1000000L;  // remainder in nanoseconds
    nanosleep(&req, NULL);                 // actually sleep
}

// gives me a running timer in milliseconds that doesn’t reset
static long long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

int main(void)
{
    srand((unsigned)time(NULL));   // seed RNG for random delays

    // try to start joystick first — if it fails, just stop here
    if (joystick_init() != 0) {
        fprintf(stderr, "Joystick init failed.\n");
        return 1;
    }

    // initialize both LEDs (turn off triggers, start dark)
    led_init();

    // intro text
    printf("Hello embedded world, from Alidad!\n");
    printf("When the LEDs light up, press the joystick in that direction!\n");
    printf("(Press LEFT or RIGHT to exit)\n");

    long long best_ms = 0; // track my best (fastest) reaction time

    // main game loop — runs until quit or timeout
    while (1) {
        printf("\nGet ready...\n");

        // flash both LEDs back and forth 4 times so user can get ready
        for (int i = 0; i < 4; ++i) {
            led_set(LED_GREEN, true);
            sleep_ms(250);
            led_set(LED_GREEN, false);

            led_set(LED_RED, true);
            sleep_ms(250);
            led_set(LED_RED, false);
        }

        // make sure joystick is centered before we start counting
        int told_release = 0;
        while (joystick_active()) {
            if (!told_release) {
                printf("Please let go of joystick.\n");
                told_release = 1;
            }
            sleep_ms(50);
        }

        // wait for random time between 0.5–3s (adds suspense)
        int delay_ms = 500 + (rand() % 2501);
        sleep_ms(delay_ms);

        // if user cheats and presses early, call them out and restart
        if (joystick_active()) {
            printf("Too soon!\n");
            continue;
        }

        // randomly choose which LED to show (up = green, down = red)
        int pick_up = rand() & 1;
        if (pick_up) {
            printf("Press UP now!\n");
            led_set(LED_GREEN, true);
        } else {
            printf("Press DOWN now!\n");
            led_set(LED_RED, true);
        }

        // start timing how long user takes to react
        long long t0 = now_ms();
        js_dir_t dir = JS_NONE;

        // give them up to 5 seconds to react
        while ((now_ms() - t0) < 5000) {
            dir = joystick_direction();
            if (dir != JS_NONE)
                break;   // joystick moved — got a direction
            sleep_ms(30); // check about 30 times per second
        }

        long long elapsed = now_ms() - t0;
        led_all_off();   // LEDs off before showing results

        // if 5 seconds pass and nothing was pressed, bail out
        if (dir == JS_NONE) {
            printf("No input within 5000ms; quitting!\n");
            break;
        }

        // left or right is my quit shortcut
        if (dir == JS_LEFT || dir == JS_RIGHT) {
            printf("User selected to quit.\n");
            break;
        }

        // figure out if the player pressed the correct direction
        int correct = (pick_up && dir == JS_UP) || (!pick_up && dir == JS_DOWN);
        if (correct) {
            printf("Correct!\n");

            // if this attempt was faster than the last best, update it
            if (best_ms == 0 || elapsed < best_ms) {
                best_ms = elapsed;
                printf("New best time!\n");
            }

            // show the numbers for this round
            printf("Your reaction time was %lldms; best so far in game is %lldms.\n",
                   elapsed, best_ms);

            // blink green LED 5 times in one second (each blink = 100ms on/off)
            led_blink(LED_GREEN, 5, 100);
        } else {
            printf("Incorrect.\n");
            // if user pressed wrong way, flash red instead
            led_blink(LED_RED, 5, 100);
        }
    }

    // cleanup before exiting (turn LEDs off and close SPI)
    led_cleanup();
    joystick_cleanup();
    return 0;
}
