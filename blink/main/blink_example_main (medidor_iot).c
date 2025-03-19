#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <math.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <esp_task_wdt.h>
#include "esp_spiffs.h"

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

// Máximo de coletas para geração do arquivo .csv
#define MAX_COLETAS 3 // Número máximo de coletas antes de imprimir a lista

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
char alertas[37] = ""; // Área para armazenar os alertas
char t_inicio[20] = ""; // Área para armazenar o timestamp de início
char t_fim[20] = ""; // Área para armazenar o timestamp de fim
char inicio_periodo[20] = ""; // Área para armazenar o inicio
int alerta_ativo = 0; // Flag para indicar se o alerta está ativo
float acumulado_periodo = 0;

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

// Variáveis para geração do arquivo .csv
float lista_consumo[MAX_COLETAS];
int current_index = 0;

static const char *TAG = "HTTP_SERVER";
float corrente_rms = 0;
float corrente[NO_OF_VALUES] = {};
float vetor_corrente[NO_OF_VALUES] = {};
uint16_t result = 0;

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
char html_page[] = "<!DOCTYPE HTML><html>\n"                       // documento com padrão HTML5
                   "<head>\n"                       // Inicia o cabeçalho
                   "  <meta charset=\"UTF-8\">"
                   "  <title>ESP32 Server - Monitoramento do Consumo de Energia</title>\n"        // Título da página
                   "  <meta http-equiv=\"refresh\" content=\"10\">\n"                // Atualiza página a cada 30s
                   "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"         // Layout adaptável à largura da tela
                  // "  <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">\n"          // Importa a biblioteca Font Awesome para usar ícones
                   // "  <link rel=\"icon\" href=\"data:,\">\n"      // Define o ícone exibido na aba do navegador
                   "  <link rel=\"icon\" href=\"favicon.ico\">"
                   "  <style>\n"
                   "    html {font-family: Arial; display: inline-block; text-align: center;}\n"     // Define fonte arial, centraliza o texto
                   "    p {  font-size: 1.2rem;}\n"                                                  // Define o tamanho da fonte
                   "    body {  margin: 0;}\n"                                                      
                   "    .topnav { overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem; }\n"        // Configura o cabeçalho da página com cor específica, texto branco e tamanho de fonte
                   "    .content { padding: 20px; }\n"               // Adiciona espaçamento
                   "    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }\n"        // Define o fundo branco e uma leve sombra para os cartões
                   "    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }\n"
                   "    .reading { font-size: 2.8rem; }\n"       // Tamanho do texto dos valores
                   "    .card.energy { color: #0e7c7b; }\n"    // Define cor do card
                   "    .card.power { color: #17bebb; }\n"       // Define cor do card
                   "    .card.voltage { color: #17bebb; }\n"       // Define cor do card
                   "    .card.current { color: #17bebb; }\n"            // Define cor do card
                   "    .card.accumulated { color: #FF5733; }\n"   // Define cor do card
                   "  </style>\n"
                   "</head>\n"                                      // Finaliza o cabeçalho
                   "<body>\n"                                       // Inicia o conteúdo visível da página
                   "  <div class=\"topnav\">\n"
                   "    <h3>ESP32 Server - Monitoramento do Consumo de Energia</h3>\n"     // Exibe o título da página no cabeçalho
                   "  </div>\n"
                   "  <div class=\"content\">\n"            // Conteúdos a serem visualizados
                   "    <div class=\"cards\">\n"            // Inicia a seção dos cartões
                   "      <div class=\"card energy\">\n"      // Cria um cartão para exibir energia consumida
                   "        <h4><i class=\"fas fa-bolt\"></i> ENERGIA</h4><p><span class=\"reading\">%.2fWh</span></p>\n"     // Título do cartão com o ícone de um raio
                   "      </div>\n"
                   "      <div class=\"card power\">\n"         // Cria um cartão para exibir a potência
                   "        <h4> POTÊNCIA</h4><p><span class=\"reading\">%.2fW</span></p>\n"     // Título do cartão com o ícone
                   "      </div>\n"     //
                   "      <div class=\"card voltage\">\n"         // Cria um cartão para exibir a tensão
                   "        <h4> TENSÃO</h4><p><span class=\"reading\">%.2fV</span></p>\n"     // Título do cartão com o ícone
                   "      </div>\n"
                   "      <div class=\"card currente\">\n"         // Cria um cartão para exibir a corrente
                   "        <h4> CORRENTE</h4><p><span class=\"reading\">%.2fA</span></p>\n"     // Título do cartão com o ícone
                   "      </div>\n"
                   "      <div id=\"alerts\" style=\"color: red; font-size: 1.5rem; padding: 10px;\">\n"
                   "        <!-- Alertas serão inseridos aqui -->\n"
                   "        %s\n" // Alertas dinâmicos
                   "      </div>\n"
                   "    </div>\n"
                   "    <div class=\"card accumulated\">\n" // Cartão para consumo acumulado
                   "      <h4>Consumo de energia entre [%s] e [%s]</h4>\n" // Exibe o intervalo de tempo
                   "      <p><span class=\"reading\">%.2fWh</span></p>\n" // Exibe o consumo acumulado
                   "    </div>\n"       // Fecha as divisões
                   "  </div>\n"         //
                   "</body>\n"          // Finaliza o corpo da página
                   "</html>";           // Finaliza o documento HTML

// Definição dos parâmetros de WI-FI                   
#define EXAMPLE_ESP_WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_EXAMPLE_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY

/* FreeRTOS event group para sinalizar status da conexão*/
static EventGroupHandle_t s_wifi_event_group;

/* Event group - bits para representar os dois estados possíveis do Wi-Fi:
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

const char html_page_2[] = "<!DOCTYPE html>\
<html>\
<head><title>Página de Teste</title></head>\
<body><h1>Funcionando!</h1></body>\
</html>";

// Configuração do Servidor Web
esp_err_t send_web_page(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entrou na função send_web_page");

    ESP_LOGI(TAG, "O Handler foi chamado! Enviando resposta...");
    esp_err_t response = httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG, "Resposta enviada. Código: %d", response);
    return response;

}
    // esp_err_t response;   // Variável para armazenar o código de resposta ao enviar a página
    // char response_data[sizeof(html_page) + 8196];   // Declara um array de caracteres (response_data) que irá armazenar os dados HTML da resposta. O tamanho é definido como o tamanho da variável html_page (uma string com o HTML) mais 256 bytes extras.
    
    // // Log para confirmar o tamanho de 'response_data' alocado
    // ESP_LOGI(TAG, "Tamanho alocado para response_data: %d bytes", sizeof(response_data));
    
    
    // memset(response_data, 0, sizeof(response_data));   // inicializa a memória do array response_data com zero para grantir que o array comece vazio e não contenha dados residuais.
    
    // // Log para verificar se a memória foi zerada corretamente
    // ESP_LOGI(TAG, "Memória de response_data inicializada.");  

    // // Montando a página web com HTML e variáveis
    // ESP_LOGI(TAG, "Montando a página HTML com as variáveis: energia = %f, potencia = %f, voltage_rms = %f, corrente_rms = %f, alertas = %d, t_inicio = %s, t_fim = %s, acumulado_periodo = %f",
    //     energia, potencia, voltage_rms, corrente_rms, alertas, t_inicio, t_fim, acumulado_periodo);
  
    // sprintf(response_data, "<html><body><h1>Teste</h1></body></html>");
    // sprintf(response_data, html_page, energia, potencia, voltage_rms, corrente_rms, alertas, t_inicio, t_fim, acumulado_periodo); // Formata a string html_page, inserindo os valores das variáveis nos espaços reservados dentro do HTML
    
    // // Log para confirmar que a página foi montada
    // ESP_LOGI(TAG, "Página HTML montada com sucesso.");    
    
    // response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);   // httpd_resp_send envia a resposta ao cliente HTTP

     // Log para verificar o código de resposta
    // if (response == ESP_OK) {
    //     ESP_LOGI(TAG, "Página enviada com sucesso.");
    // } else {
    //     ESP_LOGE(TAG, "Erro ao enviar a página. Código de erro: %s", esp_err_to_name(response));
    // }

//     return response;    // O valor retornado pela função httpd_resp_send (código de erro ou sucesso) é armazenado na variável response.
// }

// Função que retorna a página web quando requisitada - send_web_page foi configurada acima
esp_err_t get_req_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Entrou na função get_req_handler");
    ESP_LOGI(TAG, "Requisição recebida no servidor!");
    return send_web_page(req);
}

// Configuração do Manipulador de URI
httpd_uri_t uri_get = {
    .uri = "/",         // "/" indica que o manipulador responderá às requisições feitas à raiz do servidor, ou seja, http://<IP_DO_SERVIDOR>/
    .method = HTTP_GET, // Requisições do tipo GET, utilizadas para solicitar dados do servidor
    .handler = get_req_handler,  // Aponta para get_req_handler, responsável por processar a requisição e gerar a resposta adequada
    .user_ctx = NULL};

// A função setup_server configura e inicializa o servidor HTTP
httpd_handle_t setup_server(void)
{
    ESP_LOGI(TAG, "Entrou na função setup_server\n");
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    printf("Tentando iniciar o servidor HTTP na porta: %d\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "Servidor HTTP iniciado na porta: %d", config.server_port);
        // Registrar o manipulador de URI
        if (httpd_register_uri_handler(server, &uri_get) == ESP_OK)
        {
            ESP_LOGI(TAG,"Handler URI registrado com sucesso.\n");

        }
        else
        {
            ESP_LOGE(TAG,"Falha ao registrar handler URI.\n");
            return ESP_FAIL; /////// OBSERVAR A RETIRADA DESSE RETURN
        }
    }
    else 
    {
        ESP_LOGE(TAG,"Falha ao iniciar o servidor HTTP.\n");
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
        alerta_ativo = 0;

    } else if (afundamento == 1) {
        strcpy(alertas, "Afundamento de Tensão Detectado!!!");
        flag_afun = 1;
        alerta_ativo = 0;

    } else if (sobretensao == 1) {
        strcpy(alertas, "Sobretensão Detectada!!!");
        flag_sobr = 1;
        alerta_ativo = 0;

    } else if (flag_int == 1 || flag_afun == 1 || flag_sobr == 1) {
        strcpy(alertas, "Anomalia Mitigada!!!");
        alerta_ativo = 1;
        flag_int = 0;
        flag_afun = 0;
        flag_sobr = 0;
    }
}

// #define TAG "SPIFFS"

// Estrutura para armazenar pares (timestamp, energia)
typedef struct {
    char timestamp[20];
    float energia_save;
} Coleta;

// Lista de coletas
Coleta listaColetas[MAX_COLETAS];
int contadorColetas = 0;

// Mutex para proteger a energia
SemaphoreHandle_t mutexEnergia;

float energia_sav = 0.0;


// // Inicialização do SPIFFS
// void init_spiffs() {
//     esp_vfs_spiffs_conf_t conf = {
//         .base_path = "/spiffs",
//         .partition_label = NULL,
//         .max_files = 5,
//         .format_if_mount_failed = true
//     };

//     // Monta o sistema de arquivos SPIFFS
//     esp_err_t ret = esp_vfs_spiffs_register(&conf);
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Falha ao montar o SPIFFS (%s)", esp_err_to_name(ret));
//         return;
//     }

//     ESP_LOGI(TAG, "SPIFFS inicializado com sucesso.");
// }


// Função para obter timestamp
void obterTimestamp(char *buffer, size_t tamanho) {
    time_t agora = time(NULL); // Obtém o tempo atual em segundos
    struct tm *infoTempo = localtime(&agora); // Converte para o tempo local

    if (infoTempo != NULL) {
            // Formata o timestamp no formato "YYYY-MM-DD HH:MM:SS"
            strftime(buffer, tamanho, "%Y-%m-%d %H:%M:%S", infoTempo);
    } else {
            // Em caso de erro, insere uma mensagem padrão no buffer
            snprintf(buffer, tamanho, "Erro no tempo");
    }
}


// // Função para salvar as coletas em um arquivo CSV
// void salvarColetasCSV(const char *nomeBase) {
//     char nomeArquivo[50]; // Buffer para o nome do arquivo
//     char timestamp[20];   // Buffer para o timestamp

//     // Obtém o timestamp atual e formata como parte do nome do arquivo
//     obterTimestamp(timestamp, sizeof(timestamp));
//     snprintf(nomeArquivo, sizeof(nomeArquivo), "%s_%s.csv", nomeBase, timestamp);

//     // Substitui caracteres inválidos no nome do arquivo (como ':')
//     for (char *ptr = nomeArquivo; *ptr; ptr++) {
//         if (*ptr == ':' || *ptr == ' ' || *ptr == '-') {
//             *ptr = '_';
//         }
//     }

//     printf("Nome do arquivo CSV: %s\n", nomeArquivo);  // Verifique o nome gerado
//     //mostrarDiretorioAtual();

//     FILE *arquivo = fopen(nomeArquivo, "w");
//     if (arquivo == NULL) {
//         perror("Erro ao criar o arquivo CSV");
//         return;
//     }

//     // Escreve o cabeçalho
//     fprintf(arquivo, "timestamp,kWh fornecido\n");

//     // Escreve os dados das coletas
//     for (int i = 0; i < contadorColetas; i++) {
//         fprintf(arquivo, "%s,%.6f\n", listaColetas[i].timestamp, listaColetas[i].energia_save);
//     }

//     fclose(arquivo);
//     ESP_LOGI(TAG, "Arquivo CSV '%s' criado com sucesso.\n", nomeArquivo);
// }


// Função que será executada pela tarefa
// void server_task(void *pvParameter)
// {
//     ESP_LOGI(TAG, "Servidor HTTP iniciado");
//     httpd_handle_t server = setup_server();  // Configura e inicializa o servidor HTTP
    
//     // O servidor continuará funcionando até que a tarefa seja excluída
//     while (1)
//     {
//         vTaskDelay(pdMS_TO_TICKS(1000));  // A tarefa irá aguardar 1 segundo
//     }
    
//     // Parar o servidor HTTP quando a tarefa for excluída, se necessário
//     if (server != NULL) {
//         httpd_stop(server);
//     }
// }


void task_Energia(void *pvParameters) {

    TickType_t inicio = xTaskGetTickCount(); // Armazena o tempo de início em ticks
    const TickType_t periodo = pdMS_TO_TICKS(24 * 60 * 60 * 1000); // Converte 24 HORAS para ticks
    char timestamp[20]; // Buffer para armazenar o timestamp

    obterTimestamp(inicio_periodo, sizeof(inicio_periodo));

    while (1) {

        // Verifica se o tempo passou
        if ((xTaskGetTickCount() - inicio >= periodo)) {

            // Protege a variável energia com mutex
            if (xSemaphoreTake(mutexEnergia, portMAX_DELAY)) {
                energia_sav += energia;

                // Obtém o timestamp atual
                obterTimestamp(timestamp, sizeof(timestamp));

                // Adiciona os dados à lista
                strcpy(listaColetas[contadorColetas].timestamp, timestamp);
                listaColetas[contadorColetas].energia_save = energia;
                
                printf("Energia acumulada: %.2f\n", energia);
                energia = 0.0; // Reinicia o acumulador
                
                contadorColetas++;

                xSemaphoreGive(mutexEnergia);
            }

            // Verifica se a lista atingiu o número máximo de coletas
            if (contadorColetas >= MAX_COLETAS) {

                acumulado_periodo = 0.0;
                strcpy(t_inicio, "");
                strcpy(t_fim, "");

                // Imprime a lista de coletas
                printf("\n--- Lista de Coletas ---\n");
                for (int i = 0; i < contadorColetas; i++) {
                    printf("[%d] Timestamp: %s | Energia: %.6f\n", 
                           i + 1, listaColetas[i].timestamp, listaColetas[i].energia_save);
                }
                printf("--- Fim da Lista ---\n\n");

                acumulado_periodo = energia_sav;
                strcpy(t_inicio, inicio_periodo);
                obterTimestamp(t_fim, sizeof(t_fim));

                // Reinicia o contador e a lista
                contadorColetas = 0;
                strcpy(inicio_periodo, t_fim);
                energia_sav = 0.0;

            }

            inicio = xTaskGetTickCount();
            vTaskDelay(pdMS_TO_TICKS(1200));
        }

        // Delay curto para não monopolizar a CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


// Declaração da função que é executada em loop no core 0 do esp32
void task_coleta_processamento(void *pvParameter)
{

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

    gettimeofday(&start, NULL); 
    
    // static int last_second = -1;  // Armazena o último segundo processado

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

    	}
        

        media_v /= NO_OF_VALUES;
        media_i /= NO_OF_VALUES;

        for (int s = 0; s < NO_OF_VALUES; s++) {
            
            voltage_rms += (tensao[s]-media_v)*(tensao[s]-media_v);
            corrente_rms += (corrente[s]-media_i)*(corrente[s]-media_i);

    	}
            
        voltage_rms = 1075*sqrt(voltage_rms/NO_OF_VALUES); // Relação de transformação da tensão (220V/0,210) - 0,210V é o valor de saída do módulo sensor de tensão.
        verifica_tensao(voltage_rms); // Verifica possíveis afundamentos e sobretensões
                
        corrente_rms = 1.40*sqrt(corrente_rms/NO_OF_VALUES); 
        corrente_rms /= 5;
        corrente_rms *= 5;
        // 1.440 fator multiplicativo pois o sinal de corrente após o sensor passa por um circuito gerador de offset que atenua o sinal da corrente em 0,71. Logo, para obter o sinal de corrente original deve-se multiplicar a corrente eficaz calculada por 1/0,71 = 1,408
        // 5 - Fator multiplicativo pelo fato do sensor de corrente ser 5A/1V. /5 - O valor coletado da corrente é 5x o valor real, pois o cabo dá 5 voltas no sensor.
        potencia = voltage_rms*corrente_rms*0.92;

        gettimeofday(&end, NULL);
        tempo = time_diff(&start, &end);// tempo em segundos
        gettimeofday(&start, NULL);        

        tempo /= 3600;

        // Protege a variável energia com mutex
        if (xSemaphoreTake(mutexEnergia, portMAX_DELAY)) {

            energia += potencia*tempo*1;

            xSemaphoreGive(mutexEnergia);
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // Aguardar 1 segundo

        
        //////////////////////////////////// FUNÇÃO PARA AVALIAR TEMPO TRANSCORRIDO ////////////////////////////////////////////////
        if (timeinfo.tm_sec == 00 || timeinfo.tm_sec == 10 || timeinfo.tm_sec == 20 || timeinfo.tm_sec == 30 || timeinfo.tm_sec == 40 || timeinfo.tm_sec == 50) {
        // if (timeinfo.tm_sec % 10 == 0 && timeinfo.tm_sec != last_second) {
            // last_second = timeinfo.tm_sec;  // Atualiza o último segundo processado
            printf("Energia: %.2f\n",energia);
            printf("Potencia: %.2f\n",potencia);
            printf("Tensão: %.2f\n",voltage_rms);
            printf("Corrente: %.2f\n",corrente_rms);

            vTaskDelay(30000 / portTICK_PERIOD_MS); // Aguardar 1 segundo


            // flag4 = 1;
                // add_consumo(energia);
            // for (int k = 0; k < 20; k++) {
            //     verifica_tensao(voltage_rms=15*k);
            //     printf("Energia: %.2f\n",(energia=6*k));
            //     printf("Potencia: %.2f\n",(potencia=4*k));
            //     printf("Tensão: %.2f\n",(voltage_rms=15*k));
            //     printf("Corrente: %.2f\n",(corrente_rms=8*k));
            //     //transporte = energia;
            //     flag3 = 0;
            //     //energia = 0;
            //     vTaskDelay(3000 / portTICK_PERIOD_MS); // Aguardar 1,5 segundos

            // }
        }

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

    // Cria o mutex
    mutexEnergia = xSemaphoreCreateMutex();
    if (mutexEnergia == NULL) {
        printf("Erro ao criar o mutex.\n");
        return;
    }

    // xTaskCreatePinnedToCore(&hello_task_core_1, "core1_task", 1024*4, NULL, configMAX_PRIORITIES - 1, NULL, core_1);
	xTaskCreatePinnedToCore(&task_coleta_processamento, "core0_task", 4096, NULL, configMAX_PRIORITIES - 1, NULL, core_0);
    // Cria a tarefa para monitorar energia
    xTaskCreate(task_Energia, "TarefaEnergia", 4096, NULL, 4, NULL);
    // Cria a tarefa do servidor HTTP
    // xTaskCreate(server_task, "Server_Task", 16384, NULL, 5, NULL);

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
