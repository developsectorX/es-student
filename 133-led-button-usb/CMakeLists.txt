cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)

include(pico_sdk_import.cmake)

project(133_led_button_usb)

pico_sdk_init()

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

pico_set_linker_script(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/memmap_rp2040.ld
)

# Обязательно линкуем pico_stdlib
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
)

# Включаем вывод через USB (создает COM-порт) и выключаем UART
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Генерирует .uf2 файл для прошивки
pico_add_extra_outputs(${PROJECT_NAME})
