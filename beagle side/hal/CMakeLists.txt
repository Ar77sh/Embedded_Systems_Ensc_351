add_library(hal STATIC
    src/PWM0.c
    src/servo.c
    src/rotary.c
)

target_include_directories(hal PUBLIC
    ${PROJECT_SOURCE_DIR}/hal/include
)

# link libm for llround()
find_library(MATH_LIBRARY m)
if (MATH_LIBRARY)
  target_link_libraries(hal PUBLIC ${MATH_LIBRARY})
else()
  message(STATUS "libm not found explicitly; assuming math symbols in libc")
endif()

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hal PRIVATE -Wall -Wextra -Wpedantic)
endif()
