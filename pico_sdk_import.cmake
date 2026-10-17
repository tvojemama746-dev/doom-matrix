# This file lets CMake find a Pico SDK supplied through -DPICO_SDK_PATH.
if (NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif ()

if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Pass -DPICO_SDK_PATH=<path-to-pico-sdk>.")
endif ()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)