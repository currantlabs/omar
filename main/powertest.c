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
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

// Omar headers:
#include "powertest.h"
#include "hw_setup.h"

static bool echoserver_ipaddr_resolved = false;
static ip_addr_t echoserver_ipaddr;

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

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{

    printf("DNS!!\n");
    echoserver_ipaddr_resolved = true;

    if (ipaddr == NULL) {
        printf("%s(): DNS returned a null ip_addr_t pointer! Aborting...\n", __func__);
        return;
    } else {
        ip_addr_t server_ipaddr = *ipaddr;
        printf("%s(): DNS found the ip address - %i.%i.%i.%i\n",
               __func__,
               ip4_addr1(&server_ipaddr.u_addr.ip4),
               ip4_addr2(&server_ipaddr.u_addr.ip4),
               ip4_addr3(&server_ipaddr.u_addr.ip4),
               ip4_addr4(&server_ipaddr.u_addr.ip4));
    }


}

#define MESSAGE     ("hellohello")


static void tcpip_echo_task(void *arg)
{
    char ip_addr[32] = {0};
    
    // First, get the IP address for the echo server URL:
    dns_gethostbyname(ECHOSERVER_NAME, &echoserver_ipaddr, dns_found_cb, NULL );

    while (!echoserver_ipaddr_resolved) {
        printf("Waiting on DNS...\n");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }

    printf("Wow! We got the DNS server to tell us the ip address?\n");
    vTaskDelay(3000 / portTICK_PERIOD_MS);


    /* sprintf(ip_addr, "%i.%i.%i.%i",  */
    /*         ip4_addr1(&echoserver_ipaddr.u_addr.ip4), */
    /*         ip4_addr2(&echoserver_ipaddr.u_addr.ip4), */
    /*         ip4_addr3(&echoserver_ipaddr.u_addr.ip4), */
    /*         ip4_addr4(&echoserver_ipaddr.u_addr.ip4)); */

    /* printf("%s(): Got the echo server's IP address - [%s]\n", __func__, ip_addr); */

    while (1) {
        printf("%s(): stalling...\n", __func__);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    while (1) {
        struct sockaddr_in tcpServerAddr;
        tcpServerAddr.sin_addr.s_addr = inet_addr(ip_addr);
        tcpServerAddr.sin_family = AF_INET;
        tcpServerAddr.sin_port = htons( 3010 );
        int s, r;
        char recv_buf[64];

        // Hang out till you allocate a socket..
        while((s=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("%s(): Failed to allocate socket.\n", __func__);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        printf("%s(): allocated socket...\n", __func__);

        // Hang out till you successfully connect:
        while (connect(s, 
                       (struct sockaddr *)&tcpServerAddr, 
                       sizeof(tcpServerAddr)) != 0) {
            printf("%s(): socket connect failed errno=%d \n", __func__, errno);
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        printf("%s(): connected to server...\n", __func__);

        // Ping the echo server forever...
        while (1) {

            // Send a bit of data to the echo server:
            if( write(s , MESSAGE , strlen(MESSAGE)) < 0)
            {
                printf("%s(): Send failed... \n", __func__);
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                break;
            }
            printf("%s(): Send succeeded [%s]\n", __func__, MESSAGE);

            // Read back the response:
            do {
                bzero(recv_buf, sizeof(recv_buf));
                r = read(s, recv_buf, sizeof(recv_buf)-1);
                for(int i = 0; i < r; i++) {
                    putchar(recv_buf[i]);
                }
            } while(r > 0);
            printf("%s(): Done reading from socket. Last read return=%d errno=%d\r\n", __func__, r, errno);

            vTaskDelay(5000 / portTICK_PERIOD_MS);

        }
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
