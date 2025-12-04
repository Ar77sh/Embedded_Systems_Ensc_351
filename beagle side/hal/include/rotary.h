#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdbool.h>

int  rotaryEncoder_init(void);
void rotaryEncoder_cleanup(void);

// Signed value: increments CW, decrements CCW
int  rotaryEncoder_get_position(void);

// Return true exactly once per debounced button press
bool rotaryEncoder_button_pressed(void);

// Set logical position (optional helper)
void rotaryEncoder_set_position(int v);

#endif
