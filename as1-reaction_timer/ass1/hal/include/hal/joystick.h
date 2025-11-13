#ifndef HAL_JOYSTICK_H
#define HAL_JOYSTICK_H

// Simple list of possible joystick directions.
// JS_NONE = centered (no input)
// The rest match the obvious physical directions.
typedef enum {
    JS_NONE = 0,
    JS_UP,
    JS_DOWN,
    JS_LEFT,
    JS_RIGHT
} js_dir_t;

// --- Setup and teardown ---
// Opens and sets up the SPI interface for the joystick’s ADC.
int  joystick_init(void);

// Closes SPI cleanly when the program ends.
void joystick_cleanup(void);

// --- Input reading ---
// Returns 1 if the joystick is pushed far enough in any direction
// (i.e., not sitting in the middle deadzone).
int  joystick_active(void);

// Figures out which direction the joystick is being pressed toward.
// If it’s near center, returns JS_NONE.
js_dir_t joystick_direction(void);

#endif  // HAL_JOYSTICK_H
