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
#include "powertest.h"
#include "hw_setup.h"

static void tcpip_echo_task(void *arg);
static xTaskHandle tcpip_echo_task_handle = NULL;


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
        tcpip_echo_task_start_up();

        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

        // Stop talking to the server:
        tcpip_echo_task_shut_down();

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

static bool wifi_join(const char* ssid, const char* pass, int timeout_ms)
{
    wifi_config_t wifi_config = { 0 };
    strncpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strncpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
            1, 1, timeout_ms / portTICK_PERIOD_MS);
    return (bits & CONNECTED_BIT) != 0;
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


    // Attempt to connect to the AP:
    wifi_join(POWERTEST_SSID, POWERTEST_PASSWORD, 0);

    while (1){
        printf("\n\t\t\t ==> powertest up and running!\n");
        vTaskDelay(10000/portTICK_PERIOD_MS);
    }

}

static void tcpip_echo_task(void *arg)
{
    for (;;) {
        printf(".");
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}


void tcpip_echo_task_start_up(void)
{
    if (tcpip_echo_task_handle == NULL) {
        xTaskCreate(tcpip_echo_task, "AppT", 2048, NULL, 10, &tcpip_echo_task_handle);
    } else {
        printf("\n\n\n\t%s(): Error! Attempting to create a task that is already running!\n\n\n",
               __func__);
    }

    return;
}

void tcpip_echo_task_shut_down(void)
{
    if (tcpip_echo_task_handle) {
        vTaskDelete(tcpip_echo_task_handle);
        tcpip_echo_task_handle = NULL;
    }
}

#endif //defined(POWERTEST)
