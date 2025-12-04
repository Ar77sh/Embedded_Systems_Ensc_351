#ifndef HAL_PWM0_H
#define HAL_PWM0_H

int  PWM0_init(double freq_hz, double duty);   // enable @ freq (Hz) and duty [0..1]
int  PWM0_set_duty(double duty);               // change duty [0..1]
int  PWM0_set_freq(double freq_hz);            // change frequency (Hz), keep duty %
void PWM0_cleanup(void);                       // disable

#endif
