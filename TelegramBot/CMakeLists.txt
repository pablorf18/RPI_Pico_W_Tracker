add_library(TelegramBot STATIC
    TelegramBot.cpp
    TelegramBot.h
)

target_include_directories(TelegramBot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(TelegramBot
    Sim7670G
)