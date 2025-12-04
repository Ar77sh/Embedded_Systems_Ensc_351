#include "rotary.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ENC_A_GPIO  439
#define ENC_B_GPIO  336
#define ENC_SW_GPIO 434

static atomic_int g_pos = 0;
static atomic_int g_run = 0;
static atomic_int g_button_edge = 0;   // <-- set on debounced rising edge
static pthread_t  g_thread;

static int fd_a = -1, fd_b = -1, fd_sw = -1;

/* ---------- tiny sysfs helpers ---------- */
static int sysfs_write(const char *path, const char *s)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t n = write(fd, s, strlen(s));
    close(fd);
    return (n == (ssize_t)strlen(s)) ? 0 : -1;
}

static int sysfs_read_int(int fd)
{
    if (fd < 0) return -1;
    char buf[16];
    lseek(fd, 0, SEEK_SET);
    int n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return (buf[0] == '0') ? 0 : 1;
}

static int gpio_export(int n)
{
    char p[64];
    snprintf(p, sizeof(p), "/sys/class/gpio/gpio%d", n);

    // FIXED TYPO: F_OK is correct
    if (access(p, F_OK) == 0) return 0;   // already exported

    char s[16];
    snprintf(s, sizeof(s), "%d", n);
    return sysfs_write("/sys/class/gpio/export", s);
}

static int gpio_set_dir_in(int n)
{
    char p[96];
    snprintf(p, sizeof(p), "/sys/class/gpio/gpio%d/direction", n);
    return sysfs_write(p, "in");
}

static int gpio_open_value(int n)
{
    char p[96];
    snprintf(p, sizeof(p), "/sys/class/gpio/gpio%d/value", n);
    return open(p, O_RDONLY);
}

/* ---------- encoder thread ---------- */
static void *encoder_thread(void *unused)
{
    (void)unused;

    int a = sysfs_read_int(fd_a);
    int b = sysfs_read_int(fd_b);
    int last = (a<<1) | b;

    int last_sw = sysfs_read_int(fd_sw);
    struct timespec last_sw_time = {0};

    while (atomic_load(&g_run)) {
        usleep(1000); // ~1 kHz polling

        a = sysfs_read_int(fd_a);
        b = sysfs_read_int(fd_b);
        int state = (a<<1) | b;

        // Gray-code transitions: 00->01->11->10->00 : +1 (and reverse is -1)
        int diff = (last<<2) | state;
        switch (diff) {
            case 0x1: case 0x7: case 0xE: case 0x8: // +1
                atomic_fetch_add(&g_pos, 1); break;
            case 0x2: case 0x4: case 0xD: case 0xB: // -1
                atomic_fetch_sub(&g_pos, 1); break;
            default: break; // ignore glitches
        }
        last = state;

        // Button: rising-edge with ~50ms debounce
        int sw = sysfs_read_int(fd_sw);
        if (sw != last_sw) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long long dt_ms =
                (now.tv_sec - last_sw_time.tv_sec)*1000LL +
                (now.tv_nsec - last_sw_time.tv_nsec)/1000000LL;
            last_sw_time = now;
            last_sw = sw;
            if (sw == 1 && dt_ms > 50) {
                atomic_store(&g_button_edge, 1); // <-- publish press
            }
        }
    }
    return NULL;
}

/* ---------- public API ---------- */
int rotaryEncoder_init(void)
{
    if (atomic_load(&g_run)) return 0;

    if (gpio_export(ENC_A_GPIO) != 0 ||
        gpio_export(ENC_B_GPIO) != 0 ||
        gpio_export(ENC_SW_GPIO) != 0) {
        perror("rotEnc export"); return -1;
    }
    if (gpio_set_dir_in(ENC_A_GPIO) != 0 ||
        gpio_set_dir_in(ENC_B_GPIO) != 0 ||
        gpio_set_dir_in(ENC_SW_GPIO) != 0) {
        perror("rotEnc dir"); return -1;
    }
    fd_a  = gpio_open_value(ENC_A_GPIO);
    fd_b  = gpio_open_value(ENC_B_GPIO);
    fd_sw = gpio_open_value(ENC_SW_GPIO);
    if (fd_a < 0 || fd_b < 0 || fd_sw < 0) {
        perror("rotEnc open value"); return -1;
    }

    atomic_store(&g_pos, 0);
    atomic_store(&g_button_edge, 0);
    atomic_store(&g_run, 1);
    if (pthread_create(&g_thread, NULL, encoder_thread, NULL) != 0) {
        perror("rotEnc pthread_create");
        atomic_store(&g_run, 0);
        return -1;
    }
    return 0;
}

void rotaryEncoder_cleanup(void)
{
    if (!atomic_exchange(&g_run, 0)) return;
    pthread_join(g_thread, NULL);
    if (fd_a  >= 0) close(fd_a);
    if (fd_b  >= 0) close(fd_b);
    if (fd_sw >= 0) close(fd_sw);
    fd_a = fd_b = fd_sw = -1;
}

int rotaryEncoder_get_position(void)
{
    return atomic_load(&g_pos);
}

void rotaryEncoder_set_position(int v)
{
    atomic_store(&g_pos, v);
}

bool rotaryEncoder_button_pressed(void)
{
    // return true once per debounced press
    return atomic_exchange(&g_button_edge, 0) != 0;
}
