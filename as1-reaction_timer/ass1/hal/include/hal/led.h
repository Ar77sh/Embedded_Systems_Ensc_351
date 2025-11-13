#ifndef HAL_LED_H
#define HAL_LED_H

#include <stdbool.h>

// Enum to pick which LED we’re working with.
// Green = ACT LED, Red = PWR LED on the BeagleY-AI board.
typedef enum {
    LED_GREEN = 0,
    LED_RED
} led_t;

// Set up both LEDs so we can control them manually.
// (turns off system triggers like heartbeat and starts with LEDs off)
void led_init(void);

// Turns a single LED on or off depending on 'on' being true/false.
void led_set(led_t which, bool on);

// Makes an LED blink a few times.
// 'times' = how many blinks, 'half_ms' = how long it's on or off per half cycle.
// So one full blink = on + off = 2 * half_ms total.
void led_blink(led_t which, int times, int half_ms);

// Quickly turns both LEDs off — used when switching states or exiting.
void led_all_off(void);

// Cleans up on shutdown (turns both LEDs off and leaves board in a safe state).
void led_cleanup(void);

#endif  // HAL_LED_H
