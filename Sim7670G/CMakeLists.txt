add_library(Sim7670G STATIC
    sim7670g.cpp
    sim7670g.h
)

target_include_directories(Sim7670G PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Sim7670G
    pico_stdlib
    hardware_uart
    hardware_gpio
)