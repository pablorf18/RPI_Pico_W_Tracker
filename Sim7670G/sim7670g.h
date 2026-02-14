#ifndef SIM7670G_H
#define SIM7670G_H

#include <stdint.h>
#include <string>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// Configuración UART1
#define SIM7670G_UART uart1
#define SIM7670G_UART_ID 1
#define SIM7670G_TX_PIN 4      // GPIO 4
#define SIM7670G_RX_PIN 5      // GPIO 5
#define SIM7670G_BAUD 115200

// Timeouts (ms)
#define SIM7670G_CMD_TIMEOUT 5000
#define SIM7670G_INIT_TIMEOUT 10000

// Buffer sizes
#define RX_BUFFER_SIZE 4096
#define TX_BUFFER_SIZE 2048

// Estados del módulo
enum sim7670g_state_t 
{
    SIM7670G_STATE_IDLE,
    SIM7670G_STATE_INITIALIZING,
    SIM7670G_STATE_READY,
    SIM7670G_STATE_ERROR
};

// Información del módulo
struct sim7670g_info_t
{
    sim7670g_state_t state;
    char imei[16];
    char imsi[16];
    int signal_quality;  // 0-31
    bool sim_ready;
    bool network_registered;
    bool gprs_attached;
    bool pdp_active;
};

class Sim7670G 
{
public:

    explicit Sim7670G(const std::string& sim_pin);
    ~Sim7670G();

    // Funciones públicas
    void sim7670g_uart_init();
    bool sim7670g_init();
    bool sim7670g_send_command(const char *cmd, const char *expected_response, uint32_t timeout);
    bool sim7670g_check_sim();
    bool sim7670g_check_signal();
    bool sim7670g_attach_gprs();
    bool sim7670g_activate_pdp();
    bool sim7670g_get_info(sim7670g_info_t *info);
    void sim7670g_reset();
    bool sim7670g_gnss_power_on();
    bool sim7670g_gnss_power_off();
    bool sim7670g_gnss_get_location(double *lat, double *lon);
    void sim7670g_gnss_check_power();
    bool sim7670g_https_get(const char* url, char* response_buffer, int buffer_len);
    bool sim7670g_https_post(const char* url, const char* json_data, char* response_buffer, int buffer_len);

private:
    // Funciones internas
    void sim7670g_tx_string(const char *str);
    void sim7670g_rx_flush();

    sim7670g_info_t device_info;
    std::string pin_;
};

#endif