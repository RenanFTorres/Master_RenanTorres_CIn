/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO12 12 //  Branco
#define BLINK_GPIO13 13 //  Azul
#define BLINK_GPIO14 14 //  Verde
#define BLINK_GPIO27 27 //  Amarelo


static uint8_t gpio12_state = 1;
static uint8_t gpio13_state = 1;
static uint8_t gpio14_state = 1;
static uint8_t gpio27_state = 1;
static uint64_t numero = 0;

#ifdef CONFIG_BLINK_LED_RMT
static led_strip_t *pStrip_a;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (gpio_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        pStrip_a->set_pixel(pStrip_a, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        pStrip_a->refresh(pStrip_a, 100);
    } else {
        /* Set all LED off to clear all pixels */
        pStrip_a->clear(pStrip_a, 50);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    pStrip_a = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 1);
    /* Set all LED off to clear all pixels */
    pStrip_a->clear(pStrip_a, 50);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(int BLINK_GPIO,int gpio_state)
{
    /* Seta o GPIO de acordo com o estado (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, gpio_state);
}

static void configure_led(int BLINK_GPIO)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Configura o GPIO como saída */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#endif

int sort()
{
    int number;
    //printf("intervalo da rand: [0,%d]\n", RAND_MAX);
    srand( (unsigned)time(NULL) );
        
    number = rand() % 104;   // Gera um número entre 0 e 19
    //for(i=1 ; i <= 10 ; i++)
    printf("Numero: %d\n", number);
    return number;
}


void app_main(void)
{

    /* Configura o periférico */
    configure_led(BLINK_GPIO12); 
    configure_led(BLINK_GPIO13);  
    configure_led(BLINK_GPIO14);  
    configure_led(BLINK_GPIO27);  

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", gpio13_state == true ? "ON" : "OFF");
        numero = sort();
        
        if (numero == 0) {  // 0W - todas as lâmpadas apagadas
            gpio12_state = 1;
            gpio14_state = 1;
            gpio27_state = 1;
            gpio13_state = 1;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("0W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero == 1) {  // 55W - Lâmpadas verde e branca acesas
            gpio12_state = 0;
            gpio14_state = 0;
            gpio27_state = 1;
            gpio13_state = 1;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("55W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero == 2 || numero == 3) {  // 100W - Lâmpada amarela acesa
            gpio12_state = 1;
            gpio14_state = 1;
            gpio27_state = 0;
            gpio13_state = 1;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("100W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero > 3 && numero < 12) {  // 130W - Lâmpadas amarela, verde e azul acesas
            gpio12_state = 1;
            gpio14_state = 0;
            gpio27_state = 0;
            gpio13_state = 0;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("130W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero > 11 && numero < 20) {  // 140W - Lâmpadas amarela e branca acesas
            gpio12_state = 0;
            gpio14_state = 1;
            gpio27_state = 0;
            gpio13_state = 1;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("140W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero > 19 && numero < 79) {  // 155W - Lâmpadas amarela, verde e branca acesas
            gpio12_state = 0;
            gpio14_state = 0;
            gpio27_state = 0;
            gpio13_state = 1;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("155W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        if (numero > 78 && numero < 104) {  // 170W - Lâmpadas amarela, verde, branca e azul acesas
            gpio12_state = 0;
            gpio14_state = 0;
            gpio27_state = 0;
            gpio13_state = 0;
            blink_led(BLINK_GPIO14,gpio14_state);
            blink_led(BLINK_GPIO13,gpio13_state);
            blink_led(BLINK_GPIO12,gpio12_state);
            blink_led(BLINK_GPIO27,gpio27_state);
            printf("170W\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}
