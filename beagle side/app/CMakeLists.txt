cmake_minimum_required(VERSION 3.10)

# I build the main application for the trash sorter.
add_executable(main
    src/main.c
    ../hal/src/rotary.c
    ../hal/src/servo.c
    ../hal/src/PWM0.c
)

# I make sure the compiler can see the HAL headers.
target_include_directories(main PRIVATE
    ../hal/include
)

# I link against pthread and libm (for llround in PWM0.c).
target_link_libraries(main PRIVATE
    pthread
    m
)
