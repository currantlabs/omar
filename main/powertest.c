/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * powertest.c - code to run continuously, making omar burn as much power as possible
 */

#if defined(POWERTEST)


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"
#include "esp_event_loop.h"
#include "freertos/timers.h"

// Omar headers:
#include "hw_setup.h"

static EventGroupHandle_t wifi_event_group;
static const int CONNECTED_BIT = BIT0;
static const int DISCONNECTED_BIT = BIT1;

static xTimerHandle led_blink_timer;
static void led_blink_cb(xTimerHandle xTimer);
static const portTickType LED_BLINK_TIMER_PERIOD = (1000 / portTICK_RATE_MS);

static void led_blink_cb(xTimerHandle xTimer)
{
    static bool on = false;

    on = !on;

    gpio_set_level(OMAR_WHITE_LED0, on);
    gpio_set_level(OMAR_WHITE_LED1, on);


}


static void blink_leds(bool yes)
{
    if (yes) {
        xTimerStart(led_blink_timer, 0);
    } else {
        xTimerStop(led_blink_timer, 0);
    }
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

        // Stop blinking the LEDs once you're connected to the AP:
        blink_leds(false);

        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

        // If you've been disconnected, start blinking LEDs again:
        blink_leds(true);

        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );
    initialized = true;
}


static void initialize_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void powertest(void)
{
    printf("Verion %s on branch \"%s\", built on %s\n", OMAR_VERSION, OMAR_BRANCH, OMAR_TIMESTAMP);

    printf("powertest started:\n");

    led_blink_timer = xTimerCreate("led_blink_timer", LED_BLINK_TIMER_PERIOD, pdTRUE, NULL, led_blink_cb);

    initialize_nvs();
    omar_setup();

    // Start blinking the leds to indicate
    // that you're not connected - then, start
    // up the wifi:
    blink_leds(true);
    initialise_wifi();


    

    while (1){
        printf("powertesting...\n");
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }

}

#endif //defined(POWERTEST)
