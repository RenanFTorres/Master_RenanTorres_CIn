#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <esp_task_wdt.h>

// #include para NTP time
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"
#include "esp_wifi.h"

// #include para página WEB

#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>
#include "esp_http_server.h"
#include "esp_netif.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h> 

// #define para esp32/adc/dual core
#define core_0 0
#define core_1 1

#define DEFAULT_VREF    2450        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   16          // Multisampling
#define NO_OF_VALUES    110          // Coletas

// #define ADC2_EXAMPLE_CHANNEL    CONFIG_EXAMPLE_ADC2_CHANNEL //#define <nome_da_constante> <valor>
#define ADC2_EXAMPLE_CHANNEL    7 // #define <nome_da_constante> <valor>

// #define um valor que representa que a variável "transporte" está vazia
#define TRANSPORT_EMPTY -1.0f  // Valor especial para indicar "vazio"

// 3define duração do alerta de mitigação na página web
#define ALERT_DURATION_MS 30000 // Duração do alerta em milissegundos (30s)


SemaphoreHandle_t time_sync_semaphore;

TickType_t alerta_start_time = 0; // Tempo em que o alerta foi ativado

// Variável global para acumular energia
float energia_3s = 0.0;

// Inicialização WiFi e SNTP
static void obtain_time(void); 
static void initialize_sntp(void);

// Declaração de variáveis para esp32/adc/dual core
float transporte = TRANSPORT_EMPTY;
uint32_t contagem = 0;

float energia = 0;
float energia_hr = 0;
float custo = 0;
float somatorio = 0;
float potencia = 0;
float voltage = 0;
float current = 0;
float tempo = 0;
float saida = 0;
uint8_t afundamento = 0;
uint8_t interrupcao = 0;
uint8_t sobretensao = 0;
uint8_t flag0 = 0;
uint8_t flag1 = 0;
uint8_t flag2 = 0;
uint8_t flag4 = 0;
uint8_t flag_int = 0;
uint8_t flag_sobr = 0;
uint8_t flag_afun = 0;  
char alertas[512] = ""; // Área para armazenar os alertas
int alerta_ativo = 0; // Flag para indicar se o alerta está ativo

uint32_t adc_reading = 0;
uint32_t adc2_reading = 0;
float voltage_rms = 0;
float Vrms = 0;
float Irms = 0;
float media_v = 0;
float media_i = 0;
uint32_t pos = 0;
uint32_t neg = 0;

float tensao[NO_OF_VALUES] = {};
float vetor_tensao[NO_OF_VALUES] = {};
float vetor_transporte[NO_OF_VALUES] = {};
float dados_energia[][4] = {};

static const char *TAG = "Main";
float corrente_rms = 0;
float corrente[NO_OF_VALUES] = {};
float vetor_corrente[NO_OF_VALUES] = {};
uint16_t result = 0;
uint32_t current_sec = 0;
uint32_t last_sec_executed = -1;

float energia_pg = 0;
float potencia_pg = 0;
float tensao_pg = 0;
float corrente_pg = 0;

// Declaração de variáveis para NTP
time_t now; 
time_t tempo0; 
time_t tempo1; 
struct tm timeinfo; //Struct com as infomações de data e hora
int flag3 = 0;
int passagem = 0;
int mes = 0;
int ano = 0;


// ------------- WebServer START --------------------

char html_page[] = "<!DOCTYPE HTML><html>\n"
                   "<head>\n"
                   "  <meta charset=\"UTF-8\">\n"
                   "  <title>ESP32 Server - Monitoramento via WebSocket</title>\n"
                   "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                   "  <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" "
                   "        integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">\n"
                   "  <link rel=\"icon\" href=\"favicon.ico\">\n"
                   "  <style>\n"
                   "    html {font-family: Arial; display: inline-block; text-align: center;}\n"
                   "    p {font-size: 1.2rem;}\n"
                   "    body {margin: 0;}\n"
                   "    .topnav {overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem;}\n"
                   "    .content {padding: 20px;}\n"
                   "    .card {background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);}\n"
                   "    .cards {max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));}\n"
                   "    .reading {font-size: 2.8rem;}\n"
                   "    .card.energy {color: #0e7c7b;}\n"
                   "    .card.power {color: #17bebb;}\n"
                   "    .card.voltage {color: #17bebb;}\n"
                   "    .card.current {color: #17bebb;}\n"
                   "  </style>\n"
                   "</head>\n"
                   "<body>\n"
                   "  <div class=\"topnav\">\n"
                   "    <h3>ESP32 Server - Monitoramento do Consumo de Energia</h3>\n"
                   "  </div>\n"
                   "  <div class=\"content\">\n"
                   "    <div class=\"cards\">\n"
                   "      <div class=\"card energy\">\n"
                   "        <h4><i class=\"fas fa-bolt\"></i> ENERGIA</h4>\n"
                   "        <p><span class=\"reading\" id=\"energy\">0.00 Wh</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card power\">\n"
                   "        <h4>POTÊNCIA</h4>\n"
                   "        <p><span class=\"reading\" id=\"power\">0.00 W</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card voltage\">\n"
                   "        <h4>TENSÃO</h4>\n"
                   "        <p><span class=\"reading\" id=\"voltage\">0.00 V</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card current\">\n"
                   "        <h4>CORRENTE</h4>\n"
                   "        <p><span class=\"reading\" id=\"current\">0.00 A</span></p>\n"
                   "      </div>\n"
                   "    </div>\n"
                   "  </div>\n"
                   "  <script>\n"
                   "    const ws = new WebSocket('ws://' + window.location.host + '/ws');\n"
                   "    ws.onmessage = function(event) {\n"
                   "      const data = JSON.parse(event.data);\n"
                   "      document.getElementById('energy').textContent = data.energy.toFixed(2) + ' Wh';\n"
                   "      document.getElementById('power').textContent = data.power.toFixed(2) + ' W';\n"
                   "      document.getElementById('voltage').textContent = data.voltage.toFixed(2) + ' V';\n"
                   "      document.getElementById('current').textContent = data.current.toFixed(2) + ' A';\n"
                   "    };\n"
                   "  </script>\n"
                   "</body>\n"
                   "</html>";


// Definição dos parâmetros de WI-FI                   
#define EXAMPLE_ESP_WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_EXAMPLE_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY

/* FreeRTOS event group para sinalizar status da conexão*/
static EventGroupHandle_t s_wifi_event_group;

/* Event group bits para representar os dois estados possíveis do Wi-Fi:
 * - conexão Wi-Fi foi bem-sucedida
 * - conexão Wi-Fi falhou */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Variável s_retry_num conta o número de tentativas de reconexão
static int s_retry_num = 0;

// Função para lidar com os eventos de Wi-Fi e IP
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    // Evento - início da conexão Wi-Fi
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    } // Evento - falha na conexão Wi-Fi
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {   // Tenta reconnectar até o limite de vezes definido
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } // Evento - obtenção de IP via DHCP
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));  // Imprime o IP
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void connect_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();  // Cria um novo Event Group, que será usado para monitorar o status da conexão Wi-Fi

    ESP_ERROR_CHECK(esp_netif_init());   // Inicializa a rede de interface (netif) do ESP32. ESP_ERROR_CHECK verifica se a função foi executada corretamente. Caso não, dá um erro.

    ESP_ERROR_CHECK(esp_event_loop_create_default());   // Cria event loop padrão, que é necessário para que o ESP32 possa processar eventos, como a conexão Wi-Fi ou a obtenção de IP. ESP_ERROR_CHECK é utilizada para garantir que a criação do event loop foi bem-sucedida.
    esp_netif_create_default_wifi_sta();   // Cria e configura a interface Wi-Fi no modo station, o que significa que o ESP32 atuará como um cliente, tentando se conectar a uma rede Wi-Fi existente

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();   // Variável cfg é inicializada com as configurações padrão para o Wi-Fi.
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));   // A função esp_wifi_init inicializa o subsistema Wi-Fi com as configurações fornecidas pela variável cfg

    // Declaração de instâncias para manipulação de eventos
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    // Funções que registram os manipuladores de eventos para os eventos de Wi-Fi (WIFI_EVENT) e IP (IP_EVENT).
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configuração dos dados de Wi-Fi (SSID e senha)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Configuração e Início da Conexão Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // modo station (STA) - ESP32 vai se conectar a uma rede Wi-Fi como cliente.
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); //  Configura o Wi-Fi usando as credenciais fornecidas na variável wifi_config (SSID e senha)
    ESP_ERROR_CHECK(esp_wifi_start());  // Inicia a interface Wi-Fi e começa a tentar conectar-se à rede configurada

    ESP_LOGI(TAG, "wifi_init_sta finished.");  // Registra uma mensagem indicando que o processo de inicialização do Wi-Fi foi concluído

    // Função aguarda até que um desses bits seja sinalizado (WIFI_CONNECTED_BIT ou WIFI_FAIL_BIT)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    // Verificação do Resultado da Conexão
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s NESSA REDE!",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    }
    else
    {  // Se nenhum deos bits foi ativado, isso indica um comportamento inesperado e o código loga um erro
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
}



// Configuração do Servidor Web
esp_err_t send_web_page(httpd_req_t *req)
{
    printf("Entrou na função send_web_page");
    int response;   // Variável para armazenar o código de resposta ao enviar a página
    char response_data[sizeof(html_page) + 50];   // Declara um array de caracteres (response_data) que irá armazenar os dados HTML da resposta. O tamanho é definido como o tamanho da variável html_page (uma string com o HTML) mais 50 bytes extras.
    memset(response_data, 0, sizeof(response_data));   // inicializa a memória do array response_data com zeropara grantir que o array comece vazio e não contenha dados residuais.
    sprintf(response_data, html_page); //montar pagina web com html e variaveis
    response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);   // httpd_resp_send é usada para enviar uma resposta HTTP de volta ao cliente

    return response;    // O valor retornado pela função httpd_resp_send (código de erro ou sucesso) é armazenado na variável response.
}

// Função que retorna a página web quando requisitada - send_web_page foi configurada acima
esp_err_t get_req_handler(httpd_req_t *req)
{
    printf("Entrou na função get_req_handler");
    return send_web_page(req);
}

// Configuração do Manipulador de URI
httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL};

// Configuração para usar WebSockets
esp_err_t websocket_handler(httpd_req_t *req) {
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    char response[512];
    int len = snprintf(response, sizeof(response),
             "{\"energia\":%.6f,\"potencia\":%.6f,\"voltage_rms\":%.6f,\"corrente_rms\":%.6f,\"alertas\":%d}",
             energia_pg, potencia_pg, tensao_pg, corrente_pg, alertas);

    // Verificar se o tamanho da resposta é maior que o buffer alocado
    if (len >= sizeof(response)) {
        printf("Erro: A resposta JSON é maior do que o buffer alocado!\n");
        return ESP_ERR_NO_MEM;  // Ou qualquer erro adequado
    }    
    
    ws_pkt.payload = (uint8_t *)response;
    ws_pkt.len = strlen(response);

    esp_err_t res = httpd_ws_send_frame(req, &ws_pkt);
    if (res != ESP_OK) {
        printf("Falha ao enviar WebSocket: %s\n", esp_err_to_name(res));
        return res;
    }
    return ESP_OK;

}

void register_websocket_server(httpd_handle_t server) {
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = websocket_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };

    esp_err_t res = httpd_register_uri_handler(server, &ws_uri);
    if (res == ESP_OK) {
        printf("WebSocket URI registrado com sucesso.\n");
    } else {
        printf("Falha ao registrar WebSocket URI. Código de erro: %s\n", esp_err_to_name(res));
    }
}


// A função setup_server configura e inicializa o servidor HTTP
httpd_handle_t setup_server(void)
{
    printf("Entrou na função setup_server\n");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    printf("Tentando iniciar o servidor HTTP na porta: %d\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        printf("Servidor HTTP iniciado com sucesso.\n");
        // Registrar o manipulador de URI
        if (httpd_register_uri_handler(server, &uri_get) == ESP_OK)
        {
            printf("Handler URI registrado com sucesso.\n");
        }
        else
        {
            printf("Falha ao registrar handler URI.\n");
        }

        // Registrar WebSocket
        register_websocket_server(server);


    }
    else 
    {
        printf("Falha ao iniciar o servidor HTTP.\n");
        if (errno == EADDRINUSE)
        {
            printf("Erro: Porta já em uso.\n");
        }
        else if (errno == ENOMEM)
        {
            printf("Erro: Memória insuficiente.\n");
        }
        else
        {
            printf("Erro desconhecido.\n");
        }
    }

    return server;
}

// ------------- WebServer END --------------------


void verificar_alerta() {
    // Se o alerta está ativo, verifica o tempo decorrido
    if (alerta_ativo) {
        TickType_t tempo_atual = xTaskGetTickCount();
        TickType_t tempo_decorrido = tempo_atual - alerta_start_time;

        // Se o tempo decorrido for maior que a duração do alerta, removê-lo
        if (tempo_decorrido >= pdMS_TO_TICKS(ALERT_DURATION_MS)) { // 30 segundos
            alerta_ativo = 0;   // Desativa o alerta
            strcpy(alertas, ""); // Limpa a string do alerta
            printf("Alerta removido após 30 segundos.\n");
        }
    }
}


#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL); //Atualiza a hora atual imediatamente e armazena na estrutura timeval "tv"
   ESP_LOGI(TAG, "Time is synchronized from custom code");
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED); //Define o status da sincronização de horário como completo.
}
#endif


// Configuração do esp32
static esp_adc_cal_characteristics_t *adc_chars;
#if CONFIG_IDF_TARGET_ESP32
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_channel_t channel2 = ADC_CHANNEL_7;    //GPIO35 if ADC1, GPIO27 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_12; // Resolução do ADC em 12 bits - (0 - 4095)
#elif CONFIG_IDF_TARGET_ESP32S2
static const adc_channel_t channel = ADC_CHANNEL_6;     // GPIO7 if ADC1, GPIO17 if ADC2
static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
#endif
static const adc_atten_t atten = ADC_ATTEN_DB_12;      // Escolha da atenuação do conversor. A atenuação DB_11 permite que o sinal de entrada do ADC alcance valores maiores (2450mV)
static const adc_unit_t unit = ADC_UNIT_1;             // Escolha do conversor ADC

static void check_efuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}


float time_diff(struct timeval *start, struct timeval *end) {
    return ((end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec));
    }

struct timeval start;
struct timeval end;

// Função para printar a data e hora atuais
void print_current_time() {
    time_t now;
    struct tm timeinfo;

    // Atualizar o valor de "now" e preencher "timeinfo" a cada chamada
    time(&now); // Obtém o tempo atual (segundos desde a Epoch)
    localtime_r(&now, &timeinfo); // Converte para a estrutura "tm"

    // Formata e imprime a data/hora
    printf("%.2d/%.2d/%.2d %.2d:%.2d:%.2d\n",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           timeinfo.tm_year + 1900,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);
}

// Função para verificar os limites de tensão RMS
void verifica_tensao(float voltage_rms) {
    // Inicializa os indicadores como 0
    // interrupcao = 0;
    // afundamento = 0;
    // sobretensao = 0;

    if (voltage_rms < 242.0 && voltage_rms > 198.0) {
        interrupcao = 0;
        afundamento = 0;
        sobretensao = 0;
    }

    // Verifica se a tensão está abaixo de 198V (0.9 PU)
    if (voltage_rms < 198.0) {
        if (voltage_rms < 22.0) {
            afundamento = 0;
            interrupcao = 1;
        } else {
            interrupcao = 0;
            afundamento = 1;
        }
    }
    
    // Verifica se a tensão está acima de 242V (1.1 PU)
    if (voltage_rms > 242.0) {  
        sobretensao = 1;
    }

    // Cria os alertas para monitoramento remoto
    if (interrupcao == 1) {
        strcpy(alertas, "Falta de Energia Detectada!!!");
        flag_int = 1;

    } else if (afundamento == 1) {
        strcpy(alertas, "Afundamento de Tensão Detectado!!!");
        flag_afun = 1;

    } else if (sobretensao == 1) {
        strcpy(alertas, "Sobretensão Detectada!!!");
        flag_sobr = 1;
    } else if (flag_int == 1 || flag_afun == 1 || flag_sobr == 1) {
        strcpy(alertas, "Anomalia Mitigada!!!");
        alerta_ativo = 1;
        flag_int = 0;
        flag_afun = 0;
        flag_sobr = 0;
    }
}

// Declaração da função que é executada em loop no core 0 do esp32
void hello_task_core_0(void *pvParameter)
{

    flag0 = 0;
    flag1 = 0;
    flag2 = 0;


    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC - O ADC escolhido foi o unit_1 com resolução 12 bits (width), channel 06 (GPIO34) e attenuation DB_11
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);   // Configuração da resolução da saída do ADC - 12 bits
        adc1_config_channel_atten(channel, atten);   // Configuração da atenuação do ADC - DB_11
        adc1_config_channel_atten(channel2, atten);   // Configuração da atenuação do ADC - DB_11
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC - Caracteriza o ADC com uma atenuação específica
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);

    //printf("Hello world from COREEEEEEEEE %d!\n", xPortGetCoreID());
    gettimeofday(&start, NULL); 
    
	while(1) {

        
        for (int j = 0; j < NO_OF_VALUES; j++) {

        //TENSAO ELETRICA
        

            for (int i = 0; i < NO_OF_SAMPLES; i++) {

                    adc_reading += adc1_get_raw((adc1_channel_t)channel);

           } // Fim do Multisampling

            adc_reading /= NO_OF_SAMPLES;   // Obtém a média das medições

            voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars); //Convert adc_reading to voltage in mV
            tensao[j] = voltage/1000; //  /1000 - Transforma mV em V
            media_v += voltage/1000;  //  Variável para o cálculo do valor médio de V

//            printf("%.6f\n",voltage/1000);

            adc_reading = 0;

        //CORRENTE ELETRICA

            //Multisampling - Realiza (NO_OF_SAMPLES) medições em sequência para minimizar os efeitos de ruídos.
            // É calculada a média dessas medições, que contará como apenas uma conversão AD
            for (int i = 0; i < NO_OF_SAMPLES; i++) {

                    adc_reading += adc1_get_raw((adc1_channel_t)channel2);

            } // Fim do Multisampling

            adc_reading /= NO_OF_SAMPLES;   // Obtém a média das medições

            current = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars); //Convert adc_reading to voltage in mV
            corrente[j] = current/1000;
            media_i += current/1000;  //  Variável para o cálculo do valor médio de V

            adc_reading = 0;

//            printf("%.6f\n",current/1000);
    	}
        

        media_v /= NO_OF_VALUES;
        media_i /= NO_OF_VALUES;

        for (int s = 0; s < NO_OF_VALUES; s++) {
            
            voltage_rms += (tensao[s]-media_v)*(tensao[s]-media_v);
            corrente_rms += (corrente[s]-media_i)*(corrente[s]-media_i);

    	}
            
        voltage_rms = 1090*sqrt(voltage_rms/NO_OF_VALUES); // Relação de transformação da tensão (220V/0,210) - 0,210V é o valor de saída do módulo sensor de tensão.
        verifica_tensao(voltage_rms); // Verifica possíveis afundamentos e sobretensões
                
        corrente_rms = 1.440*sqrt(corrente_rms/NO_OF_VALUES); 
        corrente_rms /= 5;
        corrente_rms *= 5;
        // 1.440 fator multiplicativo pois o sinal de corrente após o sensor passa por um circuito gerador de offset que atenua o sinal da corrente em 0,71. Logo, para obter o sinal de corrente original deve-se multiplicar a corrente eficaz calculada por 1/0,71 = 1,408
        // 5 - Fator multiplicativo pelo fato do sensor de corrente ser 5A/1V. /5 - O valor coletado da corrente é 5x o valor real, pois o cabo dá 5 voltas no sensor.
        potencia = voltage_rms*corrente_rms;

        gettimeofday(&end, NULL);
        tempo = time_diff(&start, &end);//+0.00035; // tempo em segundos
        gettimeofday(&start, NULL);        

        //printf("%.6f\n",tempo);
        tempo /= 3600;
        energia += potencia*tempo*0.92;
        //energia_3s += potencia*tempo*0.92; // Os valores eficazes são calculados a cada 12 ciclos. 1 ciclo dura 16,67ms, logo, os valores eficazes sao coletados durante 200ms.

        //vTaskDelay(10 / portTICK_PERIOD_MS); // Aguardar 1 segundo

        //////////////////////////////////// FUNÇÃO PARA AVALIAR TEMPO TRANSCORRIDO ////////////////////////////////////////////////

        current_sec = timeinfo.tm_sec;
        if ((current_sec == (last_sec_executed + 3))) {
        // if ((current_sec % 10 == 0 && current_sec != last_sec_executed)) {

            energia_pg = energia;
            potencia_pg = potencia;
            tensao_pg = voltage_rms;
            corrente_pg = corrente_rms;

                printf("%.6f\n",energia_pg += 1);

                // Atualiza o último segundo executado
                last_sec_executed = current_sec;
                // vTaskDelay(1500 / portTICK_PERIOD_MS); // Aguardar 1,5 segundos
        }
        flag3 = 1;
        voltage_rms = 0;
        corrente_rms = 0;
        potencia = 0;
        media_v = 0;
        media_i = 0;
        memset(tensao, 0, sizeof(tensao));
        memset(corrente, 0, sizeof(corrente));

    }
}


static void obtain_time_task(void *pvParameter)
{
    ESP_ERROR_CHECK(nvs_flash_init()); // Inicializa o armazenamento NVS
    ESP_ERROR_CHECK(esp_netif_init()); // Inicializa a pilha TCP/IP
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Cria o loop de eventos padrão

    ESP_ERROR_CHECK(example_connect()); // Conecta à rede Wi-Fi ou Ethernet

    initialize_sntp(); // Inicializa o SNTP

    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando a sincronização de horário... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Aguarda 2 segundos
    }

    // Atualiza o horário após a sincronização
    time(&now);
    localtime_r(&now, &timeinfo);

    // Liberar o semáforo após a sincronização
    xSemaphoreGive(time_sync_semaphore);

    ESP_ERROR_CHECK(example_disconnect()); // Desconecta da rede
    vTaskDelete(NULL); // Termina a tarefa quando a sincronização for concluída
}


void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}


// Tarefa principal do código, que cria as demais tarefas a serem executadas nos cores 0 e 1
void app_main()
{

    time_t now;
    struct tm timeinfo;

    time(&now); //Função que retorna o valor do tempo em segundos desde a Epoch
    localtime_r(&now, &timeinfo); //Função que converte o valor do tempo obtido em "now" em informações detalhadas de tempo que são armazenadas em "timeinfo"
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "O tempo ainda não está definido. Conectando ao Wi-Fi e sincronizando com NTP.");
        
        // Criar o semáforo para sinalizar quando o tempo for sincronizado
        time_sync_semaphore = xSemaphoreCreateBinary();

        // Criar a tarefa de obtenção do tempo
        xTaskCreate(obtain_time_task, "Obter Tempo", 4096, NULL, 5, NULL);

        // Aguardar até que o tempo seja sincronizado
        if (xSemaphoreTake(time_sync_semaphore, pdMS_TO_TICKS(10000)) == pdTRUE) {
            ESP_LOGI(TAG, "Tempo sincronizado com sucesso!");
        } else {
            ESP_LOGE(TAG, "Falha ao sincronizar o tempo dentro do tempo limite.");
        }

        // Atualiza o 'now' após a sincronização
        time(&now);
    }

    #ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH //Atualização suave do tempo. O erro de tempo é reduzido gradualmente usando a função adjtime. Se a diferença entre o tempo de resposta do SNTP e o tempo do sistema for grande (mais de 35 minutos), atualize imediatamente.
    else { //Sequência de ações que serão realizadas caso o método escolhido nao seja o SMOOTH
    
        // add 500 ms error to the current system time.
        // Only to demonstrate a work of adjusting method!
        {
            ESP_LOGI(TAG, "Add a error for test adjtime");
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL); //Obtem a hora atual e armazena no endereço do ponteiro tv_now 
            int64_t cpu_time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec; //Armazena o tempo em cpu_time, somando o tempo em segundos e microssegundos
            int64_t error_time = cpu_time + 500 * 1000L; // Adiciona 500ms para ajustar o método SMOOTH
            struct timeval tv_error = { .tv_sec = error_time / 1000000L, .tv_usec = error_time % 1000000L }; //Cria uma nova estrutura timeval para armazenar o valor do erro
            settimeofday(&tv_error, NULL);
        }

        ESP_LOGI(TAG, "Time was set, now just adjusting it. Use SMOOTH SYNC method.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    #endif

    char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", "EST3", 1); // Função onde o fuso-horário é definido. EST3 - Fuso Recife.
    tzset(); //Inicializada a variável TZ 
    localtime_r(&now, &timeinfo); //Função que converte o valor do tempo em segundos desde a Epoch contido em "now" e armazena os dados detalhados de tempo em "timeinfo"
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo); //Converte os dados numéricos de tempo contidos em "timeinfo" em formato string para armazenar em "strftime_buf" da forma: 'Dia da semana' Mês Dia Hora:Minutos:Segundos Mês\0.
    mes = (timeinfo.tm_mon+1);
    ano = (timeinfo.tm_year+1900);
    ESP_LOGI(TAG, "The current date/time in Recife is: %s", strftime_buf); //Imprime a string
    ESP_LOGI(TAG, "Dia %.2d", timeinfo.tm_mday); //Imprime o dia
    ESP_LOGI(TAG, "Mês %.2d", mes); 
    ESP_LOGI(TAG, "Ano %.2d", ano);
    printf("%.2d/%.2d/%.2d %.2d:%.2d:%.2d", timeinfo.tm_mday,(timeinfo.tm_mon+1),(timeinfo.tm_year+1900),timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec); //Formatação do tempo e impressão como: dia/mês/ano hora:minuto:segundo

    if (sntp_get_sync_mode() == SNTP_SYNC_MODE_SMOOTH) {
        struct timeval outdelta;
        while (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) { //Loop para ajuste da hora do sistema. Permanece no loop enquanto o ajuste não for finalizado.
            adjtime(NULL, &outdelta); //Corrige a hora para permitir a sincronização do relógio do sistema
            ESP_LOGI(TAG, "Waiting for adjusting time ... outdelta = %li sec: %li ms: %li us",
                        (long)outdelta.tv_sec,
                        outdelta.tv_usec/1000,
                        outdelta.tv_usec%1000);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
    }


    // ------------ WebServer START ----------------
     // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    connect_wifi();
    
    printf("PASSEI POR AQUI!!!!!!!!!!!!!!!!!!!!");
    ESP_LOGI(TAG,"PASSEI POR AQUI!!!!!!!!!!!!!!!!!!!!");

    setup_server(); 
    // ------------ WebServer END ---------------

    // xTaskCreatePinnedToCore(&hello_task_core_1, "core1_task", 1024*4, NULL, configMAX_PRIORITIES - 1, NULL, core_1);
	xTaskCreatePinnedToCore(&hello_task_core_0, "core0_task", 1024*4, NULL, configMAX_PRIORITIES - 1, NULL, core_0);
    // Cria a tarefa para monitorar energia
    // xTaskCreate(tarefaEnergia, "TarefaEnergia", 2048, NULL, 1, NULL);

}


static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); //Define o modo de operação SNTP
    esp_sntp_setservername(0, "pool.ntp.org"); //Define o nome do host SNTP. Os parâmetros são índice e nome do servidor
    sntp_set_time_sync_notification_cb(time_sync_notification_cb); //Define uma função que será chamada após a sincronização de horário
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH); //Define o modo de sincronização. "SNTP_SYNC_MODE_SMOOTH" Atualiza o tempo suavemente, reduzindo gradualmente o erro de tempo usando a função adjtime()
#endif
    esp_sntp_init(); //Inicia o serviço SNTP.
}