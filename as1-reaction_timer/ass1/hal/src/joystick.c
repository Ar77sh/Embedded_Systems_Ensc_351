
 
 //* Joystick driver through SPI using a 12-bit ADC 
// * Connected on /dev/spidev0.0.
 //* Channel 0 = X-axis, Channel 1 = Y-axis (change if wiring is different).
 

#include "hal/joystick.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_DEV       "/dev/spidev0.0"
#define SPI_SPEED_HZ  250000   // 250 kHz is plenty for slow joystick input
#define SPI_BITS      8
#define SPI_MODE      0

#define ADC_FS        4095     // 12-bit full-scale (0-4095)
#define ADC_MID       2048     // middle value for centered stick
#define DEADZONE_PCT  8        // 8 % dead-zone around center
#define DZ_TICKS      ((ADC_FS * DEADZONE_PCT) / 100 / 2) // half-width of that zone

static int s_fd = -1;          // file descriptor for /dev/spidev0.0

// Sends a 3-byte command and reads one 12-bit channel from the ADC.
static int read_channel(int ch)
{
    unsigned char tx[3] = {
        (unsigned char)(0x06 | ((ch & 0x04) >> 2)), // start bit + single-ended mode
        (unsigned char)((ch & 0x03) << 6),          // channel select bits
        0x00
    };
    unsigned char rx[3] = {0};

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = SPI_BITS,
        .cs_change = 0
    };

    // talk to the ADC
    if (ioctl(s_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("SPI transfer");
        return -1;
    }

    // combine the two bytes into one 12-bit result
    return ((rx[1] & 0x0F) << 8) | rx[2];
}

// sets up the SPI interface so the joystick ADC can be read
int joystick_init(void)
{
    s_fd = open(SPI_DEV, O_RDWR);
    if (s_fd < 0) {
        perror("open spidev");
        return -1;
    }

    unsigned char mode  = SPI_MODE;
    unsigned char bits  = SPI_BITS;
    unsigned int  speed = SPI_SPEED_HZ;

    if (ioctl(s_fd, SPI_IOC_WR_MODE, &mode) ||
        ioctl(s_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) ||
        ioctl(s_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)) {
        perror("configure spidev");
        close(s_fd);
        s_fd = -1;
        return -1;
    }

    return 0;
}

// closes SPI device when program ends
void joystick_cleanup(void)
{
    if (s_fd >= 0)
        close(s_fd);
    s_fd = -1;
}

// quick wrappers so I don’t repeat channel numbers
static int read_x(void) { return read_channel(0); }
static int read_y(void) { return read_channel(1); }

// checks if the stick value is inside the neutral zone
static int in_deadzone(int val)
{
    if (val < 0 || val > ADC_FS)
        return 1;  // anything crazy = ignore it
    int d = val - ADC_MID;
    return (d > -DZ_TICKS && d < DZ_TICKS);
}

// returns 1 if joystick is clearly pressed in any direction
int joystick_active(void)
{
    int x = read_x();
    int y = read_y();
    if (x < 0 || y < 0)
        return 0;
    return !(in_deadzone(x) && in_deadzone(y));
}

// figure out which way the stick is pushed
js_dir_t joystick_direction(void)
{
    int x = read_x();
    int y = read_y();
    if (x < 0 || y < 0)
        return JS_NONE;

    int dx = x - ADC_MID;
    int dy = y - ADC_MID;

    // centered: nothing happening
    if (in_deadzone(x) && in_deadzone(y))
        return JS_NONE;

    // whichever axis moved more decides the direction
    if (dx * dx > dy * dy)
        return (dx > 0) ? JS_RIGHT : JS_LEFT;
    else
        return (dy > 0) ? JS_UP : JS_DOWN;
}
