#include "sim7670g.h"
#include <cstdio>
#include <string.h>

// Buffer circular para RX
static char rx_buffer[RX_BUFFER_SIZE];
static uint16_t rx_head = 0;
static uint16_t rx_tail = 0;

// Variables de estado
static uint64_t last_response_time = 0;

Sim7670G::Sim7670G(const std::string & sim_pin)
    : pin_(sim_pin)
{
}

Sim7670G::~Sim7670G()
{
    sim7670g_send_command("AT+HTTPTERM", "OK", SIM7670G_CMD_TIMEOUT);
}

/**
 * Inicializar UART1 para comunicación con SIM7670G
 */
void Sim7670G::sim7670g_uart_init() 
{
    printf("Inicializando UART1...\n");
    
    // Inicializar UART1 con los pines especificados
    uart_init(SIM7670G_UART, SIM7670G_BAUD);
    
    // Asignar pines
    gpio_set_function(SIM7670G_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(SIM7670G_RX_PIN, GPIO_FUNC_UART);
    
    // Configurar UART
    uart_set_hw_flow(SIM7670G_UART, false, false);
    uart_set_format(SIM7670G_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(SIM7670G_UART, true);
    
    printf("UART1 inicializado a %u baudios\n", SIM7670G_BAUD);
}

/**
 * Transmitir string por UART1
 */
void Sim7670G::sim7670g_tx_string(const char *str) 
{
    if (!str) 
        return;
    
    for (int i = 0; str[i]; i++) 
    {
        uart_putc(SIM7670G_UART, str[i]);
    }
}

/**
 * Limpiar buffer RX
 */
void Sim7670G::sim7670g_rx_flush() 
{
    // Primero leer lo que hay en UART
    while (uart_is_readable(SIM7670G_UART)) 
    {
        uart_getc(SIM7670G_UART);
    }
    rx_head = 0;
    rx_tail = 0;
}

/**
 * Leer una línea del buffer (terminada en \n)
 * Ignora líneas vacías y continúa buscando
 */
static bool sim7670g_read_line_skip_empty(char *line, int max_len, uint32_t timeout_ms) 
{
    uint64_t start_time = time_us_64();
    int pos = 0;
    bool line_received = false;
    
    while ((time_us_64() - start_time) < (timeout_ms * 1000)) 
    {
        // Leer caracteres del UART
        while (uart_is_readable(SIM7670G_UART)) 
        {
            char c = uart_getc(SIM7670G_UART);
            
            if (c == '\r') 
                continue;  // Ignorar CR
            if (c == '\n') 
            {
                line[pos] = '\0';
                line_received = true;
                
                // Si la línea NO está vacía, retornar
                if (pos > 0) {
                    last_response_time = time_us_64();
                    return true;
                }
                
                // Si está vacía, resetear y buscar la siguiente línea
                pos = 0;
                continue;
            }
            
            if (pos < max_len - 1) 
            {
                line[pos++] = c;
            }
        }
        
        sleep_ms(10);
    }
    
    line[pos] = '\0';
    return false;
}

/**
 * Enviar comando AT y esperar respuesta
 */
bool Sim7670G::sim7670g_send_command(const char *cmd, const char *expected_response, uint32_t timeout) 
{
    char response[256];
    
    printf("→ Enviando: %s\n", cmd);
    
    // Limpiar buffer
    sim7670g_rx_flush();
    
    // Enviar comando
    sim7670g_tx_string(cmd);
    sim7670g_tx_string("\r\n");
    
    // Esperar respuesta
    uint64_t start_time = time_us_64();
    bool found = false;
    int line_count = 0;
    
    while ((time_us_64() - start_time) < (timeout * 1000) && line_count < 10) 
    {
        if (sim7670g_read_line_skip_empty(response, sizeof(response), 100)) 
        {
            line_count++;
            
            if (strlen(response) > 0) 
            {
                printf("← Recibido: %s\n", response);
            }
            
            // ✅ Si esperamos respuesta específica
            if (expected_response && strstr(response, expected_response)) 
            {
                printf("✓ Respuesta encontrada: %s\n", expected_response);
                return true;  // ✅ RETORNA INMEDIATAMENTE
            }
            
            // ✅ Si NO especificamos respuesta esperada, buscar OK/ERROR
            if (!expected_response) 
            {
                if (strstr(response, "OK")) 
                {
                    printf("✓ OK recibido\n");
                    return true;  // ✅ RETORNA INMEDIATAMENTE
                }
                if (strstr(response, "ERROR")) 
                {
                    printf("✗ ERROR recibido\n");
                    return false;  // ✅ RETORNA INMEDIATAMENTE
                }
            }
        }
    }
    
    // Timeout alcanzado sin encontrar respuesta
    if (expected_response) {
        printf("✗ Timeout esperando: %s\n", expected_response);
        return found;
    }
    
    printf("✗ Timeout, ni OK ni ERROR recibido\n");
    return false;
}

/**
 * Verificar estado de la tarjeta SIM
 */
bool Sim7670G::sim7670g_check_sim() 
{
    printf("Verificando tarjeta SIM...\n");
    
    if (sim7670g_send_command("AT+CPIN?", "SIM PIN", SIM7670G_CMD_TIMEOUT))
    {
        std::string pinCommand = "AT+CPIN=\"" + pin_ + "\"";
        if(sim7670g_send_command(pinCommand.c_str(), "OK", SIM7670G_CMD_TIMEOUT))
        {
            printf("✓ SIM desbloqueada\n");
        }
        else
        {
            printf("❌ Error al desbloquear SIM con PIN\n");
            return false;
        }
    }
    else if(!sim7670g_send_command("AT+CPIN?", "READY", SIM7670G_CMD_TIMEOUT))
    {
        printf("❌ SIM no lista\n");
        return false;
    }
    
    printf("✓ SIM lista\n");
    device_info.sim_ready = true;
    return true;
}

/**
 * Verificar calidad de señal
 */
bool Sim7670G::sim7670g_check_signal() 
{
    char response[128];
    int rssi = 99;
    int retries = 3;
    
    printf("Verificando señal...\n");
    
    // Reintentar hasta 3 veces
    while (retries > 0) 
    {
        sim7670g_rx_flush();
        sim7670g_tx_string("AT+CSQ\r\n");
        
        if (sim7670g_read_line_skip_empty(response, sizeof(response), 2000)) 
        {
            // Respuesta: +CSQ: rssi,ber
            if (strstr(response, "+CSQ:")) 
            {
                int ber;
                sscanf(response, "+CSQ: %d,%d", &rssi, &ber);
                device_info.signal_quality = rssi;
            
                printf("Señal: RSSI=%d (0-31)\n", rssi);
            
                if (rssi == 99) 
                {
                    printf("⚠️  Señal no detectada\n");
                    retries--;
                    sleep_ms(1000);
                    continue;
                }
                return true;
            }
        }
        else
        {
            printf("❌ Respuesta inesperada: %s\n", response);
        }
    }
    
    printf("❌ Error al leer señal\n");
    return false;
}

/**
 * Adjuntar a GPRS
 */
bool Sim7670G::sim7670g_attach_gprs() 
{
    printf("Adjuntando a GPRS...\n");
    
    if (!sim7670g_send_command("AT+CGATT=1", "OK", SIM7670G_CMD_TIMEOUT)) 
    {
        printf("❌ Error al adjuntar GPRS\n");
        return false;
    }
    
    printf("✓ GPRS adjuntado\n");
    device_info.gprs_attached = true;
    return true;
}

/**
 * Activar contexto PDP (Internet)
 */
bool Sim7670G::sim7670g_activate_pdp() 
{
    printf("Activando contexto PDP...\n");
    
    // Definir contexto PDP 1
    if (!sim7670g_send_command("AT+CGDCONT=1,\"IP\",\"internet\"", "OK", SIM7670G_CMD_TIMEOUT)) 
    {
        printf("⚠️  Error al definir contexto (continuar)\n");
    }
    
    // Activar contexto
    sleep_ms(1000);
    if (!sim7670g_send_command("AT+CGACT=1,1", "OK", SIM7670G_CMD_TIMEOUT)) 
    {
        printf("❌ Error al activar PDP\n");
        return false;
    }
    
    printf("✓ Contexto PDP activado\n");
    device_info.pdp_active = true;
    return true;
}

bool Sim7670G::sim7670g_gnss_power_on()
{
    printf("Encendiendo GNSS...\n");
    if (!sim7670g_send_command("AT+CGNSSPWR=1", "OK", SIM7670G_CMD_TIMEOUT)) // o AT+CGNSSPWR=1 según módulo
    {
        printf("❌ No se pudo encender GNSS\n");
        return false;
    }
    printf("✓ GNSS encendido\n");
    return true;
}

bool Sim7670G::sim7670g_gnss_power_off()
{
    printf("Apagando GNSS...\n");
    if (!sim7670g_send_command("AT+CGNSSPWR=0", "OK", SIM7670G_CMD_TIMEOUT))
    {
        printf("❌ No se pudo apagar GNSS\n");
        return false;
    }
    printf("✓ GNSS apagado\n");
    return true;
}

bool Sim7670G::sim7670g_gnss_get_location(double *lat, double *lon)
{
    if (!lat || !lon) return false;
    
    char response[256];
    
    printf("Consultando posición GPS...\n");
    
    // ✅ NO llamar a CGNSPWR, solo consultar información
    sim7670g_rx_flush();
    sim7670g_tx_string("AT+CGPSINFO\r\n");
    
    uint64_t start = time_us_64();
    
    while ((time_us_64() - start) < 3000000ULL) 
    {
        if (sim7670g_read_line_skip_empty(response, sizeof(response), 500)) 
        {
            printf("GPS response: %s\n", response);
            
            if (strstr(response, "+CGPSINFO:")) 
            {
                double lat_raw = 0.0;
                double lon_raw = 0.0;
                char lat_dir = 'N';
                char lon_dir = 'W';
                
                // ✅ Parser simplificado (tu módulo solo devuelve 4 campos)
                int parsed = sscanf(response,
                    "+CGPSINFO: %lf,%c,%lf,%c",
                    &lat_raw, &lat_dir, &lon_raw, &lon_dir);
                
                if (parsed >= 3 && lat_raw > 0 && lon_raw > 0) 
                {
                    // Convertir NMEA a decimales
                    double lat_degrees = (int)(lat_raw / 100);
                    double lat_minutes = lat_raw - (lat_degrees * 100);
                    double flat = lat_degrees + (lat_minutes / 60.0);
                    
                    double lon_degrees = (int)(lon_raw / 100);
                    double lon_minutes = lon_raw - (lon_degrees * 100);
                    double flon = -(lon_degrees + (lon_minutes / 60.0));
                    
                    if (lat_dir == 'S') flat = -flat;
                    if (lon_dir == 'W') flon = -flon;
                    
                    if (flat >= -90 && flat <= 90 && flon >= -180 && flon <= 180) 
                    {
                        *lat = flat;
                        *lon = flon;
                        
                        printf("✓ GPS Posición: lat=%.6f, lon=%.6f\n", flat, flon);
                        return true;
                    }
                }
                break;
            }
        }
    }
    
    printf("❌ Timeout esperando posición GPS\n");
    return false;
}

void Sim7670G::sim7670g_gnss_check_power()
{
    char response[128];
    
    printf("Verificando estado de encendido GNSS...\n");
    
    sim7670g_rx_flush();
    sim7670g_tx_string("AT+CGNSSPWR?\r\n");
    
    if (sim7670g_read_line_skip_empty(response, sizeof(response), 2000))
    {
        printf("Respuesta: %s\n", response);
        
        // Busca el formato: +CGNSSPWR: 0 o +CGNSSPWR: 1
        if (strstr(response, "+CGNSSPWR: 0"))
            printf("⚠️  GNSS está APAGADO (OFF)\n");
        else if (strstr(response, "+CGNSSPWR: 1"))
            printf("✓ GNSS está ENCENDIDO (ON)\n");
        else
            printf("❌ Respuesta inesperada\n");
    }
    else
    {
        printf("❌ Timeout consultando CGNSSPWR\n");
    }
}

/**
 * Obtener información del dispositivo
 */
bool Sim7670G::sim7670g_get_info(sim7670g_info_t *info) 
{
    char response[128];
    
    // Obtener IMEI
    sim7670g_rx_flush();
    sim7670g_tx_string("AT+GSN\r\n");
    if (sim7670g_read_line_skip_empty(response, sizeof(response), SIM7670G_CMD_TIMEOUT)) 
    {
        strncpy(device_info.imei, response, 15);
        printf("IMEI: %s\n", device_info.imei);
    }
    
    *info = device_info;
    return true;
}

bool Sim7670G::sim7670g_https_get(const char* url, char* response_buffer, int buffer_len)
{
    char response[256];
    char cmd[512];
    
    if (!url || !response_buffer) 
        return false;
    
    printf("HTTPS GET: %s\n", url);
    
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sim7670g_send_command(cmd, "OK", SIM7670G_CMD_TIMEOUT))
    {
        printf("❌ Error al configurar URL\n");
        return false;
    }
    
    // Desactivar compresión
    sim7670g_send_command("AT+HTTPPARA=\"USERDATA\",\"Accept-Encoding: identity\"", "OK", SIM7670G_CMD_TIMEOUT);
    
    // Ejecutar GET
    sim7670g_rx_flush();
    sim7670g_tx_string("AT+HTTPACTION=0\r\n");
    
    bool ok = false;
    uint64_t start = time_us_64();
    int content_length = 0;
    
    // Esperar +HTTPACTION
    while ((time_us_64() - start) < 5000000ULL) 
    {
        // ✅ CAMBIO: timeout por línea REDUCIDO a 100ms
        if (!sim7670g_read_line_skip_empty(response, sizeof(response), 100))
            continue;
        
        if (strstr(response, "+HTTPACTION:")) 
        {
            int method, status, length;
            sscanf(response, "+HTTPACTION: %d,%d,%d", &method, &status, &length);
            content_length = length;
            
            printf("HTTP Status: %d, Content-Length: %d bytes\n", status, length);
            
            if (status == 200) {
                ok = true;
            }
            break;
        }
    }

    if (!ok || content_length <= 0) {
        printf("❌ HTTP request failed\n");
        return false;
    }

    sleep_ms(500);
    sim7670g_rx_flush();
    
    snprintf(cmd, sizeof(cmd), "AT+HTTPREAD=0,%d\r\n", content_length);
    printf("Enviando: %s\n", cmd);
    sim7670g_tx_string(cmd);
    
    int pos = 0;
    int no_data_loops = 0;
    int total_expected = content_length + 100;  // +100 para header y overhead
    
    // ✅ NUEVO: Lee sin depender de uart_is_readable()
    // Lee byte a byte CON TIMEOUT individual para cada byte
    uint64_t read_start = time_us_64();
    
    while (pos < buffer_len - 1 && (time_us_64() - read_start) < 30000000ULL) 
    {
        // ✅ CLAVE: Intentar leer 1 byte CON TIMEOUT
        uint64_t byte_start = time_us_64();
        bool byte_received = false;
        
        while ((time_us_64() - byte_start) < 100000ULL) {  // 100ms timeout por byte
            if (uart_is_readable(SIM7670G_UART)) {
                char c = uart_getc(SIM7670G_UART);
                response_buffer[pos++] = c;
                byte_received = true;
                no_data_loops = 0;
                break;
            }
            sleep_ms(1);  // Sleep pequeño para no quemar CPU
        }
        
        // Si no llegó byte, incrementar contador
        if (!byte_received) {
            no_data_loops++;
            
            // ✅ CLAVE: Después de 500ms sin datos, verificar si tenemos JSON completo
            if (no_data_loops > 50) {  // 50 * 10ms = 500ms
                printf("⚠️  500ms sin datos, verificando si JSON está completo\n");
                
                // Buscar JSON válido en buffer
                char* json_start = strchr(response_buffer, '{');
                char* json_end = NULL;
                
                if (json_start) {
                    int brace_count = 0;
                    bool in_string = false;
                    bool escape_next = false;
                    
                    for (char* p = json_start; p < response_buffer + pos; p++) {
                        char c = *p;
                        
                        if (c == '"' && !escape_next) in_string = !in_string;
                        escape_next = (c == '\\' && !escape_next);
                        
                        if (!in_string) {
                            if (c == '{') brace_count++;
                            else if (c == '}') {
                                brace_count--;
                                if (brace_count == 0) {
                                    json_end = p;
                                    break;
                                }
                            }
                        }
                    }
                }
                
                // Si encontramos JSON completo, salir
                if (json_start && json_end) {
                    printf("✓ JSON completo encontrado después de %d bytes\n", pos);
                    break;
                }
                
                // Si llevamos 2 segundos sin datos, forzar salida
                if (no_data_loops > 200) {  // 200 * 10ms = 2 segundos
                    printf("❌ Timeout 2 segundos sin datos, saliendo\n");
                    break;
                }
            }
        }
    }
    
    printf("✓ Total leído: %d bytes\n", pos);
    
    // ✅ EXTRACCIÓN FINAL DE JSON POR BALANCE DE LLAVES
    char* json_start = strchr(response_buffer, '{');
    char* json_end = NULL;
    
    if (json_start) {
        int brace_count = 0;
        bool in_string = false;
        bool escape_next = false;
        
        for (char* p = json_start; p < response_buffer + pos; p++) {
            char c = *p;
            
            if (c == '"' && !escape_next) {
                in_string = !in_string;
            }
            
            escape_next = (c == '\\' && !escape_next);
            
            if (!in_string) {
                if (c == '{') brace_count++;
                else if (c == '}') {
                    brace_count--;
                    if (brace_count == 0) {
                        json_end = p;
                        break;
                    }
                }
            }
        }
    }
    
    if (json_start && json_end && json_end > json_start) {
        int json_len = json_end - json_start + 1;
        memmove(response_buffer, json_start, json_len);
        response_buffer[json_len] = '\0';
        
        printf("✓ JSON extraído: %d bytes\n", json_len);
        printf("Preview: %.*s\n", 300, response_buffer);
        
        ok= true;
    } 
    else 
    {
        printf("❌ JSON válido no encontrado en respuesta (%d bytes leídos)\n", pos);
        printf("Respuesta completa: %s\n", response_buffer);
        ok = false;
    }

    return ok;
}

bool Sim7670G::sim7670g_https_post(const char* url, const char* json_data, char* response_buffer, int buffer_len)
{
    char response[256];
    char cmd[768];
    
    if (!url || !json_data || !response_buffer) return false;
    
    printf("HTTPS POST: %s\n", url);
    printf("Data: %s\n", json_data);
    
    // URL
    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);
    if (!sim7670g_send_command(cmd, "OK", 5000))
    {
        return false;
    }

    // Content-Type JSON
    if (!sim7670g_send_command("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 5000))
    {
        return false;
    }
    
    // Datos
    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000", (int)strlen(json_data));
    sim7670g_rx_flush();
    sim7670g_tx_string(cmd);
    sim7670g_tx_string("\r\n");
    
    // Esperar DOWNLOAD
    if (!sim7670g_read_line_skip_empty(response, sizeof(response), 5000) || 
        !strstr(response, "DOWNLOAD")) 
    {
        printf("❌ HTTPDATA failed\n");
        return false;
    }
    
    // Enviar datos JSON
    sim7670g_tx_string(json_data);
    
    // Ejecutar POST
    sim7670g_rx_flush();
    sim7670g_tx_string("AT+HTTPACTION=1\r\n");  // 1=POST 
    
    bool ok = false;
    uint64_t start = time_us_64();
    
    while ((time_us_64() - start) < (SIM7670G_CMD_TIMEOUT * 1000ULL)) 
    {
        if (!sim7670g_read_line_skip_empty(response, sizeof(response), 5000))
            continue;
        
        if (strstr(response, "+HTTPACTION:")) 
        {
            int method, status, length;
            sscanf(response, "+HTTPACTION: %d,%d,%d", &method, &status, &length);

            printf("HTTP POST Status: %d, Length: %d, response: %s\n", status, length, response);
            
            if (status == 200) 
            {
                ok = true;
            }
            break;
        }
    }
    
    return ok;
}

/**
 * Reiniciar módulo (hard reset)
 */
void Sim7670G::sim7670g_reset() 
{
    printf("Reiniciando SIM7670G...\n");
    sim7670g_send_command("AT+CRESET", NULL, 5000);
    sleep_ms(3000);
}

/**
 * Inicialización completa del módulo
 */
bool Sim7670G::sim7670g_init() 
{
    printf("\n====================================\n");
    printf("Iniciando SIM7670G...\n");
    printf("====================================\n");
    
    device_info.state = SIM7670G_STATE_INITIALIZING;
    
    // 1. Esperar a que el módulo esté listo
    sleep_ms(2000);
    
    // 2. Desactivar echo
    printf("[1/7] Desactivando echo...\n");
    sim7670g_send_command("ATE0", "OK", SIM7670G_CMD_TIMEOUT);
    sleep_ms(500);
    
    // 3. Verificar SIM
    printf("[2/7] Verificando SIM...\n");
    if (!sim7670g_check_sim()) 
    {
        device_info.state = SIM7670G_STATE_ERROR;
        return false;
    }
    sleep_ms(1000);
    
    // 5. Verificar señal
    printf("[3/7] Verificando señal...\n");
    if (!sim7670g_check_signal()) 
    {
        printf("⚠️  Señal débil, continuando...\n");
    }
    sleep_ms(1000);
    
    // 6. Adjuntar GPRS
    printf("[4/7] Adjuntando GPRS...\n");
    if (!sim7670g_attach_gprs()) 
    {
        device_info.state = SIM7670G_STATE_ERROR;
        return false;
    }
    sleep_ms(2000);
    
    // 7. Activar PDP
    printf("[5/7] Activando contexto PDP...\n");
    if (!sim7670g_activate_pdp()) 
    {
        device_info.state = SIM7670G_STATE_ERROR;
        return false;
    }
    sleep_ms(1000);

    // Inicializar HTTP
    printf("[6/7] Inicializando HTTP...\n");
    if (!sim7670g_send_command("AT+HTTPINIT", "OK", SIM7670G_CMD_TIMEOUT))
        return false;

    // 8. Encender GNSS y obtener posición
    printf("[7/7] Encendiendo GNSS y obteniendo posición...\n");
    double lat, lon;
    if (sim7670g_gnss_power_on())
    {
        sleep_ms(2000);
        sim7670g_gnss_check_power();
        sleep_ms(3000);
        // esperar a que consiga fix
        for (int i = 0; i < 300; i++)  // ~30 s
        {
            sleep_ms(1000);
            if (sim7670g_gnss_get_location(&lat, &lon))
            {
                printf("POSICIÓN: lat=%.6f, lon=%.6f\n", lat, lon);
                break;
            }
            else
            {
                printf("Esperando fix GNSS... (%d/30)\n", i+1);
            }
        }
    }
    
    // 8. Obtener información
    printf("[SUCCESS] Obteniendo información del dispositivo...\n");
    sim7670g_info_t info;
    sim7670g_get_info(&info);
    
    device_info.state = SIM7670G_STATE_READY;
    
    printf("\n====================================\n");
    printf("✓ SIM7670G INICIALIZADO CORRECTAMENTE\n");
    printf("====================================\n");
    printf("IMEI: %s\n", device_info.imei);
    printf("Señal: %d/31\n", device_info.signal_quality);
    printf("SIM: %s\n", device_info.sim_ready ? "LISTA" : "ERROR");
    printf("GPRS: %s\n", device_info.gprs_attached ? "ADJUNTADO" : "ERROR");
    printf("Internet: %s\n", device_info.pdp_active ? "ACTIVO" : "INACTIVO");
    printf("====================================\n");
    
    return true;
}