#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <string>
#include <functional>
#include "sim7670g.h"
#include <queue>

struct TelegramMessage 
{
    std::string chat_id;
    std::string text;
};

class TelegramBot 
{
public:
    using MessageCallback = std::function<void(const std::string& chat_id, 
                                               const std::string& text, 
                                               const std::string& from_username)>;

    TelegramBot(const char* bot_token, Sim7670G & sim7670g);
    ~TelegramBot();

    // Enviar mensaje de texto
    bool sendMessage(const char* chat_id, const char* text);

    // Obtener actualizaciones (polling)
    void getUpdates();

    // Registrar callback para mensajes recibidos
    void onMessage(MessageCallback callback);

    // Procesar eventos (llamar en bucle principal)
    void loop();

    // Obtener ubicación GNSS
    bool getLocation(double *lat, double *lon);

    // Habilitar o deshabilitar modo activo
    bool enableActiveMode(bool enable);

private:
    std::string bot_token;
    Sim7670G sim7670g;
    MessageCallback message_callback;
    int32_t last_update_id;
    uint32_t last_poll_time;
    bool waiting_response;
    std::queue<TelegramMessage> message_queue;

    void parse_updates(const std::string& json_response);
    std::string extract_json_field(const std::string& json, const char* field);
    bool send_queued_messages();

    uint32_t telegramPollInterval = 45000; // Intervalo de polling en ms
};

#endif // TELEGRAM_BOT_H
