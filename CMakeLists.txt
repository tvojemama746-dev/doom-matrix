cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(tiny_doom_16x8 C CXX ASM)

pico_sdk_init()

add_executable(tiny_doom_16x8
    src/main.c
    src/display_ws2812.c
    src/input.c
    src/sound.c
    src/world.c
    src/raycaster.c
)

pico_generate_pio_header(tiny_doom_16x8 ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(tiny_doom_16x8 PRIVATE src)

target_link_libraries(tiny_doom_16x8
    pico_stdlib
    hardware_clocks
    hardware_gpio
    hardware_pio
    hardware_pwm
)

# GPIO0 and GPIO1 are game buttons, not UART pins.
pico_enable_stdio_uart(tiny_doom_16x8 0)
pico_enable_stdio_usb(tiny_doom_16x8 0)
pico_add_extra_outputs(tiny_doom_16x8)