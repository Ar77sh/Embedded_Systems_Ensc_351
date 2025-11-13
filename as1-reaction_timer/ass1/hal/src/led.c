/*
 * LED control for the BeagleY-AI board.
 * ACT LED = green, PWR LED = red.
 * These are controlled through sysfs entries under /sys/class/leds.
 * I basically turn off the kernel “triggers” so I can blink them manually.
 */

#include "hal/led.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// File paths for the two LEDs (same ones you see if you run `ls /sys/class/leds`)
#define LED_ACT_TRIGGER "/sys/class/leds/ACT/trigger"
#define LED_PWR_TRIGGER "/sys/class/leds/PWR/trigger"
#define LED_ACT_BRIGHT  "/sys/class/leds/ACT/brightness"
#define LED_PWR_BRIGHT  "/sys/class/leds/PWR/brightness"

// simple helper to write a short string to one of those sysfs files
static int write_str(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("open sysfs (LED)");   // prints a message if file doesn’t exist or permission issue
        return -1;
    }
    write(fd, val, strlen(val));      // don’t care about partial writes; tiny values anyway
    close(fd);
    return 0;
}

// returns the right brightness file depending on which LED we’re controlling
static const char *brightness_path(led_t which)
{
    return (which == LED_GREEN) ? LED_ACT_BRIGHT : LED_PWR_BRIGHT;
}

// called once at the start — disables system triggers and ensures both LEDs are off
void led_init(void)
{
    write_str(LED_ACT_TRIGGER, "none");   // remove heartbeat or disk-activity behavior
    write_str(LED_PWR_TRIGGER, "none");
    write_str(LED_ACT_BRIGHT, "0");       // both off
    write_str(LED_PWR_BRIGHT, "0");
}

// basic on/off control for a single LED
void led_set(led_t which, bool on)
{
    write_str(brightness_path(which), on ? "1" : "0");
}

// makes an LED blink a given number of times; timing is handled with usleep()
void led_blink(led_t which, int times, int half_ms)
{
    for (int i = 0; i < times; ++i) {
        led_set(which, true);
        usleep(half_ms * 1000);   // on time
        led_set(which, false);
        usleep(half_ms * 1000);   // off time
    }
}

// turns both LEDs off — I call this before showing messages or before quitting
void led_all_off(void)
{
    write_str(LED_ACT_BRIGHT, "0");
    write_str(LED_PWR_BRIGHT, "0");
}

// cleanup at the end of the program; leaves board LEDs dark
void led_cleanup(void)
{
    led_all_off();
    // I’m not restoring triggers (like heartbeat) because assignment says
    // LEDs should remain off when the program ends.
}
