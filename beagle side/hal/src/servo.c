#define _GNU_SOURCE
#include "servo.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int write_str(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;

    ssize_t n = write(fd, val, strlen(val));
    int rc = (n < 0) ? -errno : 0;
    close(fd);
    return rc;
}

static int write_int(const char *path, long long v)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", v);
    return write_str(path, buf);
}

static int exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int export_pwm(int chip, int channel)
{
    // I keep the base chip path modest, and give derived paths more room.
    char chip_path[128];
    snprintf(chip_path, sizeof(chip_path),
             "/sys/class/pwm/pwmchip%d", chip);

    if (!exists(chip_path)) {
        return -ENOENT;
    }

    char pwm_path[256];
    snprintf(pwm_path, sizeof(pwm_path),
             "%s/pwm%d", chip_path, channel);

    if (exists(pwm_path)) {
        return 0;
    }

    char export_path[256];
    snprintf(export_path, sizeof(export_path),
             "%s/export", chip_path);

    int rc = write_int(export_path, channel);
    if (rc == -EBUSY) {
        return 0;
    }

    // Wait a bit for sysfs to create the pwmN directory
    for (int i = 0; i < 50 && !exists(pwm_path); ++i) {
        struct timespec ts = {0, 20 * 1000 * 1000}; // 20 ms
        nanosleep(&ts, NULL);
    }

    return exists(pwm_path) ? 0 : rc;
}

static int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int pct_to_ns(const Servo *s, int pct, int dir)
{
    pct = clamp(pct, 0, 100);
    int span = (s->max_ns - s->neutral_ns);
    if (span < 0) span = -span;
    int delta = (span * pct) / 100;
    return (dir >= 0)
        ? (s->neutral_ns + delta)
        : (s->neutral_ns - delta);
}

int servo_init(Servo *s,
               int chip, int channel,
               int period_ns, int neutral_ns, int min_ns, int max_ns)
{
    if (!s) return -EINVAL;

    // Allow override via env PWM0_CHIP when caller passes chip<0
    if (chip < 0) {
        const char *env = getenv("PWM0_CHIP");
        if (env && *env) {
            chip = atoi(env);
        } else {
            chip = 0;
        }
    }

    memset(s, 0, sizeof(*s));
    s->chip        = chip;
    s->channel     = channel;
    s->period_ns   = period_ns;
    s->neutral_ns  = neutral_ns;
    s->min_ns      = min_ns;
    s->max_ns      = max_ns;

    int rc = export_pwm(chip, channel);
    if (rc) return rc;

    snprintf(s->base, sizeof(s->base),
             "/sys/class/pwm/pwmchip%d/pwm%d", chip, channel);

    char p_period[320], p_enable[320], p_duty[320], p_polarity[320];
    snprintf(p_period,  sizeof(p_period),  "%s/period",      s->base);
    snprintf(p_enable,  sizeof(p_enable),  "%s/enable",      s->base);
    snprintf(p_duty,    sizeof(p_duty),    "%s/duty_cycle",  s->base);
    snprintf(p_polarity,sizeof(p_polarity),"%s/polarity",    s->base);

    // Disable, prime tiny duty, set period & neutral, enable
    write_int(p_enable, 0);
    write_int(p_duty, 1);

    // If polarity exists, try "normal" first
    if (exists(p_polarity)) {
        write_str(p_polarity, "normal");
    }

    if ((rc = write_int(p_period, s->period_ns)))  return rc;
    if ((rc = write_int(p_duty,   s->neutral_ns))) return rc;
    if ((rc = write_int(p_enable, 1)))             return rc;

    s->enabled = true;
    return 0;
}

int servo_set_pulse_ns(Servo *s, int duty_ns)
{
    if (!s || !s->enabled) return -EIO;

    duty_ns = clamp(duty_ns, s->min_ns, s->max_ns);

    char p_duty[320];
    snprintf(p_duty, sizeof(p_duty), "%s/duty_cycle", s->base);

    return write_int(p_duty, duty_ns);
}

int servo_right(Servo *s, int speed_pct)
{
    return servo_set_pulse_ns(s, pct_to_ns(s, speed_pct, +1));
}

int servo_left(Servo *s, int speed_pct)
{
    return servo_set_pulse_ns(s, pct_to_ns(s, speed_pct, -1));
}

int servo_stop(Servo *s)
{
    return servo_set_pulse_ns(s, s->neutral_ns);
}

int servo_close(Servo *s)
{
    if (!s) return -EINVAL;

    char p_enable[320], unexp[320], chipdir[128];

    snprintf(p_enable, sizeof(p_enable), "%s/enable", s->base);
    write_int(p_enable, 0);

    snprintf(chipdir, sizeof(chipdir),
             "/sys/class/pwm/pwmchip%d", s->chip);
    snprintf(unexp, sizeof(unexp),
             "%s/unexport", chipdir);

    write_int(unexp, s->channel);
    s->enabled = false;
    return 0;
}
