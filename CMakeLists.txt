cmake_minimum_required(VERSION 3.13)

set(PROGRAM_NAME RPI_Pico_W_Tracker)

include(pico_sdk_import.cmake)

project(${PROGRAM_NAME} C CXX ASM) 
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,--allow-multiple-definition")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--allow-multiple-definition")

add_subdirectory(Sim7670G)
add_subdirectory(TelegramBot)

add_executable(${PROGRAM_NAME}
    main.cpp
)

target_compile_definitions(${PROGRAM_NAME} PRIVATE
        TELEGRAM_BOT_TOKEN=\"${TELEGRAM_BOT_TOKEN}\"
        TELEGRAM_AUTORIZED_USERS=\"${TELEGRAM_AUTORIZED_USERS}\"
        SIM_PIN=\"${SIM_PIN}\"
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROGRAM_NAME} 0)
pico_enable_stdio_usb(${PROGRAM_NAME} 1)

target_link_libraries(${PROGRAM_NAME}
    TelegramBot
    Sim7670G
)

pico_add_extra_outputs(${PROGRAM_NAME})