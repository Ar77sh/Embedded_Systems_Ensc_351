// Minimal, warning-free PWM sysfs HAL for ENSC351 A2 (robust + fast-path)

#include <math.h>
#include "PWM0.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <glob.h>
#include <time.h>

static long long read_ll(const char *path);

// ---- tiny safe helpers -------------------------------------------------
static bool path_exists(const char *p) { struct stat st; return stat(p, &st) == 0; }

static int write_str(const char *path, const char *s)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t n = write(fd, s, (size_t)strlen(s));
    close(fd);
    return (n == (ssize_t)strlen(s)) ? 0 : -1;
}
static int write_ll(const char *path, long long v)
{
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%lld", v);
    if (n < 0 || n >= (int)sizeof(buf)) return -1;
    return write_str(path, buf);
}
static void msleep(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
// Build dst = a + b (no formatting), with bounds check. Returns 0 on ok.
static int path_join2(char *dst, size_t dstsz, const char *a, const char *b)
{
    size_t la = strnlen(a, dstsz); if (la >= dstsz) return -1;
    size_t lb = strlen(b);         if (la + lb >= dstsz) return -1;
    memcpy(dst, a, la);
    memcpy(dst + la, b, lb + 1);
    return 0;
}

// ------------------------------------------------------------------------
static char      g_pwm_dir[PATH_MAX] = {0};  // /sys/class/pwm/pwmchipX/pwm0
static long long g_period_ns = 0;
static double    g_duty_frac = 0.50;         // remember last duty ratio

/* Helpers for bounded duty (avoid rails) */
static inline long long bounded_dc_from_ratio(double ratio, long long per)
{
    if (ratio < 0.05) ratio = 0.05;
    if (ratio > 0.95) ratio = 0.95;
    long long dc = (long long) llround(ratio * (double)per);
    if (dc <= 0)       dc = 1;
    if (dc >= per)     dc = per - 1;
    return dc;
}

/* Probe any pwmchip/pwm0. */
static int probe_find_pwm(void)
{
    if (g_pwm_dir[0] != '\0') return 0;

    glob_t g = (glob_t){0};
    if (glob("/sys/class/pwm/pwmchip*", 0, NULL, &g) != 0) {
        errno = ENODEV; perror("no /sys/class/pwm/pwmchip*"); return -1;
    }

    for (size_t i = 0; i < g.gl_pathc; ++i) {
        char chip[PATH_MAX], exportf[PATH_MAX], pwm0[PATH_MAX];
        if (path_join2(chip,   sizeof(chip),   g.gl_pathv[i], "")        != 0) continue;
        if (path_join2(exportf,sizeof(exportf),chip, "/export")          != 0) continue;
        if (path_join2(pwm0,   sizeof(pwm0),   chip, "/pwm0")            != 0) continue;

        if (!path_exists(pwm0)) {
            (void)write_str(exportf, "0");
            for (int t = 0; t < 50 && !path_exists(pwm0); ++t) msleep(10);
        }
        if (!path_exists(pwm0)) continue;

        char enable[PATH_MAX], period[PATH_MAX], duty[PATH_MAX];
        if (path_join2(enable,sizeof(enable),pwm0,"/enable")     != 0) continue;
        if (path_join2(period,sizeof(period),pwm0,"/period")     != 0) continue;
        if (path_join2(duty,  sizeof(duty),  pwm0,"/duty_cycle") != 0) continue;

        if (path_exists(enable) && path_exists(period) && path_exists(duty)) {
            if (path_join2(g_pwm_dir, sizeof(g_pwm_dir), pwm0, "") != 0) {
                globfree(&g); errno = ENAMETOOLONG; return -1;
            }
            globfree(&g);
            return 0;
        }
    }

    globfree(&g);
    errno = ENODEV;
    perror("no usable pwmchip*/pwm0 found");
    return -1;
}

int PWM0_init(double hz, double duty)
{
    if (probe_find_pwm() != 0) return -1;

    char enable[PATH_MAX], period[PATH_MAX], dutyf[PATH_MAX];
    if (path_join2(enable,sizeof(enable),g_pwm_dir,"/enable")     != 0) return -1;
    if (path_join2(period,sizeof(period),g_pwm_dir,"/period")     != 0) return -1;
    if (path_join2(dutyf, sizeof(dutyf), g_pwm_dir,"/duty_cycle") != 0) return -1;

    if (hz   <= 0.0) hz   = 100.0;
    if (duty <  0.05) duty = 0.50;   // avoid edge stickiness on bring-up
    if (duty >  0.95) duty = 0.50;

    long long per_ns = (long long) llround(1e9 / hz);
    if (per_ns < 1) per_ns = 1;
    long long dty_ns = bounded_dc_from_ratio(duty, per_ns);

    // Robust bring-up sequence with retries:
    // Some drivers require: disable -> (tiny duty) -> period -> duty -> enable
    for (int attempt = 0; attempt < 3; ++attempt) {
        // 1) hard-off
        (void)write_ll(enable, 0);
        msleep(2);

        // 2) prime duty to tiny value so any new period is valid
        //    (prevents EINVAL when period < old duty)
        (void)write_ll(dutyf, 1);
        msleep(1);

        // 3) program period for requested Hz
        if (write_ll(period, per_ns) != 0) {            // <-- where your EINVAL happened
            msleep(2);
            continue;  // retry
        }

        // 4) set bounded duty for that new period
        if (write_ll(dutyf, dty_ns) != 0) {
            msleep(2);
            continue;  // retry
        }

        // 5) enable
        if (write_ll(enable, 1) != 0) {
            msleep(2);
            continue;  // retry
        }

        // Success: cache
        g_period_ns = per_ns;
        g_duty_frac = (double)dty_ns / (double)per_ns;
        return 0;
    }

    // If we got here, bring-up failed
    perror("PWM0_init");
    errno = EINVAL;
    return -1;
}


/* Fast-path freq change: try update while enabled; fallback + retries if needed. */
int PWM0_set_freq(double hz)
{
    if (g_pwm_dir[0] == '\0' || hz <= 0.0) { errno = EINVAL; return -1; }

    char enable[PATH_MAX], period[PATH_MAX], dutyf[PATH_MAX];
    if (path_join2(enable,sizeof(enable),g_pwm_dir,"/enable")     != 0) return -1;
    if (path_join2(period,sizeof(period),g_pwm_dir,"/period")     != 0) return -1;
    if (path_join2(dutyf, sizeof(dutyf), g_pwm_dir,"/duty_cycle") != 0) return -1;

    long long new_per = (long long) llround(1e9 / hz);
    if (new_per < 1) new_per = 1;

    // Always compute a bounded mid duty for the *new* period
    double      ratio   = g_duty_frac;
    long long   new_dty = bounded_dc_from_ratio(ratio, new_per);

    // If we were disabled (e.g., 0 Hz), re-enable first (no flash: we don't change duty yet)
    long long was_enabled = read_ll(enable);
    if (was_enabled == 0) {
        (void)write_ll(enable, 1);
        msleep(2); // let sysfs settle
        was_enabled = 1;
    } else if (was_enabled < 0) {
        // if we couldn't read, assume enabled
        was_enabled = 1;
    }

    // ---- FAST PATH A: period -> duty while enabled
    if (write_ll(period, new_per) == 0) {
        if (write_ll(dutyf, new_dty) == 0) {
            g_period_ns = new_per;
            g_duty_frac = (double)new_dty / (double)new_per;
            return 0;
        }
    }

    // ---- FAST PATH B (in-place safe order): set tiny duty -> period -> target duty
    // This avoids EINVAL when current duty > new period.
    if (write_ll(dutyf, 1) == 0) {
        if (write_ll(period, new_per) == 0 && write_ll(dutyf, new_dty) == 0) {
            g_period_ns = new_per;
            g_duty_frac = (double)new_dty / (double)new_per;
            return 0;
        }
    }

    // ---- FALLBACK: brief disable, program, re-enable
    for (int attempt = 0; attempt < 2; ++attempt) {
        (void)write_ll(enable, 0);
        msleep(1);

        // Prime away from rails, then program
        (void)write_ll(dutyf, 1);
        if (write_ll(period, new_per) == 0 && write_ll(dutyf, new_dty) == 0) {
            if (write_ll(enable, 1) != 0) continue;
            msleep(1);
            g_period_ns = new_per;
            g_duty_frac = (double)new_dty / (double)new_per;
            return 0;
        }

        // back-off and try once more
        msleep(1);
        (void)write_ll(enable, 1);
        msleep(1);
    }

    perror("PWM0_set_freq: could not program");
    errno = EINVAL;
    return -1;
}

int PWM0_set_duty(double duty)
{
    if (g_pwm_dir[0] == '\0') { errno = EINVAL; return -1; }

    if (g_period_ns <= 0) {
        char period[PATH_MAX];
        if (path_join2(period,sizeof(period),g_pwm_dir,"/period") != 0) return -1;
        long long p = read_ll(period);
        if (p <= 0) { errno = EINVAL; return -1; }
        g_period_ns = p;
    }

    g_duty_frac = duty;
    char dutyf[PATH_MAX];
    if (path_join2(dutyf,sizeof(dutyf),g_pwm_dir,"/duty_cycle") != 0) return -1;

    long long dc = bounded_dc_from_ratio(g_duty_frac, g_period_ns);

    // try direct, fallback via brief disable if EINVAL
    if (write_ll(dutyf, dc) != 0) {
        char enable[PATH_MAX];
        if (path_join2(enable,sizeof(enable),g_pwm_dir,"/enable") != 0) return -1;
        long long was_enabled = read_ll(enable);
        if (was_enabled < 0) was_enabled = 1;
        (void)write_ll(enable, 0);
        msleep(1);
        (void)write_ll(dutyf, 1);
        int ok = (write_ll(dutyf, dc) == 0);
        if (was_enabled) (void)write_ll(enable, 1);
        if (!ok) { perror("set duty_cycle"); return -1; }
    }
    return 0;
}

void PWM0_cleanup(void)
{
    if (g_pwm_dir[0] == '\0') return;
    char enable[PATH_MAX];
    if (path_join2(enable,sizeof(enable),g_pwm_dir,"/enable") == 0) {
        (void)write_str(enable, "0");
    }
}

// ---- helper ------------------------------------------------------------
static long long read_ll(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long long v = -1;
    if (fscanf(f, "%lld", &v) != 1) { fclose(f); return -1; }
    fclose(f);
    return v;
}
