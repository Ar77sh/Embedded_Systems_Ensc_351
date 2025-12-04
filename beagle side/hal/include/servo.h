#ifndef SERVO_H
#define SERVO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int  chip;          // pwmchipN (we’ll default to 0; override via PWM0_CHIP env)
    int  channel;       // pwmM (always 0 here)
    int  period_ns;     // e.g., 20_000_000 (50 Hz)
    int  neutral_ns;    // e.g., 1_500_000
    int  min_ns;        // e.g., 1_000_000
    int  max_ns;        // e.g., 2_000_000
    char base[256];     // "/sys/class/pwm/pwmchipN/pwmM"
    bool enabled;
} Servo;

/* Initialize the servo at /sys/class/pwm/pwmchip{chip}/pwm{channel}.
   If chip < 0, we’ll read PWM0_CHIP from the environment (default 0). */
int  servo_init(Servo *s,
                int chip, int channel,
                int period_ns, int neutral_ns, int min_ns, int max_ns);

/* Convenience motions for continuous or positional servos */
int  servo_left (Servo *s, int pct);   // 0..100 (maps around neutral)
int  servo_right(Servo *s, int pct);   // 0..100
int  servo_stop (Servo *s);            // go to neutral

/* Set exact pulse width in nanoseconds. Clamped to [min_ns, max_ns]. */
int  servo_set_pulse_ns(Servo *s, int duty_ns);

/* Disable output and unexport channel. */
int  servo_close(Servo *s);

#ifdef __cplusplus
}
#endif
#endif
