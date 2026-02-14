#include "TelegramBot.h"
#include <cstdio>
#include <cstring>
#include "pico/stdlib.h"

TelegramBot::TelegramBot(const char* bot_token, Sim7670G & sim7670g) 
    : bot_token(bot_token), 
      sim7670g(sim7670g),
      last_update_id(0),
      last_poll_time(0),
      waiting_response(false)
{
}

TelegramBot::~TelegramBot() 
{
}

bool TelegramBot::sendMessage(const char* chat_id, const char* text) 
{
    if (!chat_id || !text) 
    {
        printf("[TelegramBot::sendMessage] Invalid parameters\n");
        return false;
    }

    printf("[TelegramBot] Sending message to chat %s: %s\n", chat_id, text);

    char json_buffer[512];
    snprintf(json_buffer, sizeof(json_buffer),
             "{\"chat_id\":\"%s\",\"text\":\"%s\"}",
             chat_id, text);

    char url[512];
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/sendMessage",
             bot_token.c_str());

    printf("[TelegramBot] POST URL: %s\n", url);
    printf("[TelegramBot] JSON: %s\n", json_buffer);

    // Llamada síncrona bloqueante con POST
    char * response_buffer = new char[TX_BUFFER_SIZE];
    bool ok = false;
    if (sim7670g.sim7670g_https_post(url, json_buffer, response_buffer, TX_BUFFER_SIZE)) 
    {
        printf("[TelegramBot] ✓ HTTP 200\n");
        ok = true;
    } 
    else 
    {
        printf("[TelegramBot] ❌ HTTP POST failed\n");
        
        // add to message queue for retry later
        message_queue.push({chat_id, text});
    }

    delete [] response_buffer;

    return ok;
}

bool TelegramBot::send_queued_messages() 
{
    while (!message_queue.empty()) 
    {
        TelegramMessage msg = message_queue.front();
        
        if (sendMessage(msg.chat_id.c_str(), msg.text.c_str())) 
        {
            message_queue.pop();
            sleep_ms(500);
        } 
        else 
        {
            printf("[TelegramBot] Queue message failed, stopping retry\n");
            break;
        }
    }
    return message_queue.empty();
}

bool TelegramBot::getLocation(double *lat, double *lon) 
{
    return sim7670g.sim7670g_gnss_get_location(lat, lon);
}

bool TelegramBot::enableActiveMode(bool enable) 
{
    telegramPollInterval = enable ? 2000 : 45000; // 2s or 45s
    printf("[TelegramBot] Active mode %s, poll interval set to %d ms\n", 
           enable ? "enabled" : "disabled", telegramPollInterval);
    return true;
}

void TelegramBot::getUpdates() 
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Check polling interval
    if (current_time - last_poll_time < telegramPollInterval) {
        return;
    }

    last_poll_time = current_time;

    // Construir URL con offset
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/getUpdates?offset=%d&timeout=10",
             bot_token.c_str(), last_update_id + 1);

    printf("[TelegramBot] Polling for updates (offset=%d)...\n", last_update_id + 1);

    // Llamada síncrona bloqueante
    char * response_buffer = new char[RX_BUFFER_SIZE];
    if (sim7670g.sim7670g_https_get(url, response_buffer, RX_BUFFER_SIZE)) 
    {
        printf("[TelegramBot] ✓ getUpdates HTTP 200\n");
        std::string data(response_buffer);
        parse_updates(data);
    } 
    else 
    {
        printf("[TelegramBot] ❌ getUpdates HTTP failed\n");
    }

    delete[] response_buffer;
}

void TelegramBot::parse_updates(const std::string& json_response) 
{
    // simple parsing of JSON response to extract updates
    size_t pos = 0;
    int32_t max_update_id = last_update_id;

    printf("[TelegramBot] Parsing updates json=%s\n", json_response.c_str());
    
    while ((pos = json_response.find("\"update_id\":", pos)) != std::string::npos) 
    {
        pos += 12; // update_id length
        int update_id = atoi(json_response.c_str() + pos);
        
        if (update_id > max_update_id) 
        {
            max_update_id = update_id;
        }

        // search for message
        size_t msg_start = json_response.find("\"message\":", pos);
        if (msg_start == std::string::npos || msg_start > json_response.find("\"update_id\":", pos + 1)) 
        {
            // No message found for this update_id
            continue;
        }

        // get chat id
        std::string chat_id = extract_json_field(json_response.substr(msg_start, 500), "\"id\"");
        
        // get text
        std::string text = extract_json_field(json_response.substr(msg_start, 1000), "\"text\"");
        
        // get username
        std::string username = extract_json_field(json_response.substr(msg_start, 500), "\"username\"");

        if (!chat_id.empty() && !text.empty()) 
        {
            printf("[TelegramBot] New message from @%s (chat_id=%s): %s\n", 
                   username.c_str(), chat_id.c_str(), text.c_str());
            
            if (message_callback) 
            {
                message_callback(chat_id, text, username);
            }
        }

        pos++;
    }

    // update last_update_id
    if (max_update_id > last_update_id) 
    {
        last_update_id = max_update_id;
        printf("[TelegramBot] Updated last_update_id to %d\n", last_update_id);
    }
}

std::string TelegramBot::extract_json_field(const std::string& json, const char* field) 
{
    size_t field_pos = json.find(field);
    if (field_pos == std::string::npos) 
    {
        return "";
    }

    // find the value start
    size_t value_start = json.find(':', field_pos);
    if (value_start == std::string::npos) 
    {
        return "";
    }
    value_start++;

    // skip spaces
    while (value_start < json.length() && (json[value_start] == ' ' || json[value_start] == '\t')) 
    {
        value_start++;
    }

    // The strings are enclosed in quotes
    // Handle string values enclosed in quotes
    if (json[value_start] == '"') 
    {
        value_start++; // Skip the opening quote
        std::string value;
        bool escape = false;

        for (size_t i = value_start; i < json.length(); i++) 
        {
            char c = json[i];
            if (escape) 
            {
                // Handle escaped characters
                value += c;
                escape = false;
            } 
            else if (c == '\\') 
            {
                // Escape the next character
                escape = true;
            } 
            else if (c == '"') 
            {
                // End of the string
                printf("[TelegramBot] Extracted field %s: %s\n", field, value.c_str());
                return value;
            } 
            else 
            {
                value += c;
            }
        }
        printf("[TelegramBot] unexpected end!\n");
        return ""; // Unterminated string
    }
    
    // For numeric values
    size_t value_end = json.find_first_of(",}", value_start);
    if (value_end == std::string::npos) 
    {
        return "";
    }
    
    std::string value = json.substr(value_start, value_end - value_start);
    // remove trailing spaces
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) 
    {
        value.pop_back();
    }
    
    return value;
}

void TelegramBot::onMessage(MessageCallback callback) 
{
    message_callback = callback;
}

void TelegramBot::loop() 
{
    //get telegram updates
    getUpdates();
    
    // try to send queued messages
    send_queued_messages();
}