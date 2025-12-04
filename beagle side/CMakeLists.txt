cmake_minimum_required(VERSION 3.10)
project(SunFounderServo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Make both hal/include and app/include visible globally (simple fix)
include_directories(
    ${PROJECT_SOURCE_DIR}/hal/include
    ${PROJECT_SOURCE_DIR}/app/include
)

add_subdirectory(hal)
add_subdirectory(app)
