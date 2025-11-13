add_library(hal
    src/led.c
    src/joystick.c
)

target_include_directories(hal PUBLIC include)
