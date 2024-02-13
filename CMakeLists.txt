cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

project(trafo C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

if (DEFINED ENV{PICO_EXAMPLES_PATH} AND NOT PICO_EXAMPLES_PATH)
    set(PICO_EXAMPLES_PATH $ENV{PICO_EXAMPLES_PATH})
    message("Using PICO_EXAMPLES_PATH from environment ('${PICO_EXAMPLES_PATH}')")
endif()

# Initialize the SDK
pico_sdk_init()

include(example_auto_set_url.cmake)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

add_subdirectory(rp2040_lib)

add_executable(${PROJECT_NAME}
        main.cpp
        trafo.cpp
        controlknob.cpp
        display.cpp
        )

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

target_include_directories(${PROJECT_NAME} PUBLIC rp2040_lib/src)
target_link_libraries(${PROJECT_NAME} PUBLIC rp2040_lib)

target_link_libraries(${PROJECT_NAME} PRIVATE pico_stdlib)
target_link_libraries(${PROJECT_NAME} PRIVATE hardware_pio)
target_link_libraries(${PROJECT_NAME} PRIVATE hardware_i2c)
target_link_libraries(${PROJECT_NAME} PRIVATE hardware_spi)
target_link_libraries(${PROJECT_NAME} PRIVATE hardware_pwm)
target_link_libraries(${PROJECT_NAME} PRIVATE hardware_adc)
target_link_libraries(${PROJECT_NAME} PRIVATE pico_multicore)


pico_add_extra_outputs(${PROJECT_NAME})
