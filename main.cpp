#include <stdio.h>
#include "pico/stdlib.h"
#include "TelegramBot.h"
#include "sim7670g.h"

#include <sstream>

TelegramBot* bot = nullptr;
std::vector<std::string> authorized_users;

std::string escape_special_characters(const std::string& input) 
{
    std::ostringstream escaped;
    for (char c : input) 
    {
        switch (c) 
        {
            case '\n': escaped << "\\n"; break;
            case '\t': escaped << "\\t"; break;
            case '\r': escaped << "\\r"; break;
            case '\"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

//callback to handle received messages
void on_telegram_message(const std::string& chat_id, 
                        const std::string& text, 
                        const std::string& username) 
{
    printf("\n=== New message received ===\n");
    printf("From: @%s\n", username.c_str());
    printf("Chat ID: %s\n", chat_id.c_str());
    std::string escaped_text = escape_special_characters(text);
    printf("Text: %s\n", escaped_text.c_str());
    printf("===========================\n\n");

    if(std::find(authorized_users.begin(), authorized_users.end(), chat_id) == authorized_users.end())
    {
        printf("[Warning] Message from unauthorized chat_id: %s\n", chat_id.c_str());
        return;
    }

    // ask message
    if (escaped_text == "/start") 
    {
        bot->sendMessage(chat_id.c_str(), 
            "¡Hola! Soy tu bot en Raspberry Pi Pico W.\n"
            "Comandos disponibles:\n"
            "/start - Este mensaje\n"
            "/location - Obtener ubicación actual\n"
            "/activo - Estado activo del bot\n"
            "/lowEnergy - Modo de bajo consumo\n");
    }
    else if( escaped_text == "/location") 
    {
        double lat = 0.0, lon = 0.0;
        if (bot->getLocation(&lat, &lon)) 
        {
            char location_msg[256];
            snprintf(location_msg, sizeof(location_msg), 
                     "Ubicación actual:\nLatitud: %.6f\nLongitud: %.6f", 
                     lat, lon);
            bot->sendMessage(chat_id.c_str(), location_msg);
        } 
        else 
        {
            bot->sendMessage(chat_id.c_str(), "No se pudo obtener la ubicación GNSS en este momento.");
        }
    }
    else if( escaped_text == "/activo") 
    {
        bot->enableActiveMode(true);
        bot->sendMessage(chat_id.c_str(), "Modo activo activado. El bot responderá rápidamente a los comandos.");
    }
    else if( escaped_text == "/lowEnergy") 
    {
        bot->enableActiveMode(false);
        bot->sendMessage(chat_id.c_str(), "Modo de bajo consumo activado. Tiempos de respuesta más lentos.");
    }
    else 
    {
        char response[256];
        snprintf(response, sizeof(response), 
                 "Recibí tu mensaje: %s\nEnvía /start para ver los comandos.", 
                 escaped_text.c_str());
        bot->sendMessage(chat_id.c_str(), response);
    }
}

int main() 
{
    stdio_init_all();
    
    // wait for USB serial to be ready
    sleep_ms(5000);
    
    printf("\n");
    printf("======================================\n");
    printf("  Tracker - Raspberry Pi Pico W\n");
    printf("======================================\n\n");

    Sim7670G sim7670g = Sim7670G(SIM_PIN);

    // initialize UART for SIM7670G
    sim7670g.sim7670g_uart_init();
    sleep_ms(1000);
    
    // initialize SIM7670G module
    if (!sim7670g.sim7670g_init()) 
    {
        printf("FALLO EN LA INICIALIZACIÓN DEL MÓDULO SIM7670G\n");
    }

    sleep_ms(1000);

    //tokenize the TELEGRAM_CHAT_ID to get autorized users
    std::istringstream ss(TELEGRAM_AUTORIZED_USERS);
    std::string token;
    while (std::getline(ss, token, ',')) 
    {
        if(!token.empty())
        {
            authorized_users.push_back(token);
        }
    }

    printf("\n[Main] Creating Telegram bot instance...\n");
    bot = new TelegramBot(TELEGRAM_BOT_TOKEN, sim7670g);

    // Register method to handle incoming messages
    bot->onMessage(on_telegram_message);

    printf("\n✅ Bot running! Waiting for messages...\n\n");

    while (true) 
    {
        // Process bot events
        bot->loop();
        
        // light sleep to reduce CPU usage
        sleep_ms(20);
    }

    delete bot;

    return 0;
}