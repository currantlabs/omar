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
    /* ip_addr_t server_ipaddr = *ipaddr; */
    /* echoserver_ipaddr_resolved = true; */

    /* printf("%s(): DNS found the ip address - %i.%i.%i.%i\n",  */
    /*     __func__, */
    /*     ip4_addr1(&server_ipaddr.u_addr.ip4), */
    /*     ip4_addr2(&server_ipaddr.u_addr.ip4), */
    /*     ip4_addr3(&server_ipaddr.u_addr.ip4), */
    /*     ip4_addr4(&server_ipaddr.u_addr.ip4)); */

    printf("DNS!!\n");
    echoserver_ipaddr_resolved = true;

    /* ip_addr_t mycopy = *ipaddr; */
    /* u32_t theipaddr = mycopy.u_addr.ip4.addr; */


    /* Activating the commented line gives us this crasher:
     *
     * Verion eefdb28f2db2940c94af721c8eb4931120b18bbb on branch "powertest", built on 2018-11-25 at 18:00:15
     * powertest started:
     * I (133) gpio: GPIO[32]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (133) gpio: GPIO[22]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (133) gpio: GPIO[33]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (143) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (153) gpio: GPIO[26]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (163) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * I (173) gpio: GPIO[10]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
     * s5852 successfully initialized
     * I (183) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
     * I (193) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
     * I (213) wifi: wifi driver task: 3ffc2600, prio:23, stack:3584, core=0
     * I (213) wifi: wifi firmware version: e27cdcf
     * I (213) wifi: config NVS flash: enabled
     * I (223) wifi: config nano formating: disabled
     * I (223) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
     * I (233) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
     * I (263) wifi: Init dynamic tx buffer num: 32
     * I (263) wifi: Init data frame dynamic rx buffer num: 32
     * I (263) wifi: Init management frame dynamic rx buffer num: 32
     * I (273) wifi: Init static rx buffer size: 1600
     * I (273) wifi: Init static rx buffer num: 10
     * I (273) wifi: Init dynamic rx buffer num: 32
     * I (343) phy: phy_version: 4000, b6198fa, Sep  3 2018, 15:11:06, 0, 0
     * I (343) wifi: mode : null
     * I (343) wifi: mode : sta (d8:a0:1d:63:3c:24)
     * 
     *                          ==> powertest up and running!
     * I (463) wifi: n:1 0, o:1 0, ap:255 255, sta:1 0, prof:1
     * I (1453) wifi: state: init -> auth (b0)
     * I (1453) wifi: state: auth -> assoc (0)
     * I (1453) wifi: state: assoc -> run (10)
     * I (1493) wifi: connected with Stringer 2.4, channel 1
     * I (1503) wifi: pm start, type: 1
     * 
     * I (2213) event: sta ip: 192.168.1.44, mask: 255.255.255.0, gw: 192.168.1.1
     * Waiting on DNS...
     * DNS!!
     * Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.
     * Core 1 register dump:
     * PC      : 0x400d3649  PS      : 0x00060630  A0      : 0x8012c48d  A1      : 0x3ffbf950
     * A2      : 0x3ffb6bd4  A3      : 0x00000000  A4      : 0x00000000  A5      : 0x400d3638
     * A6      : 0x00010001  A7      : 0x00010001  A8      : 0x3ffb4250  A9      : 0x00000001
     * A10     : 0x0000000a  A11     : 0x3ffc79f4  A12     : 0x3ffbf940  A13     : 0x00000001
     * A14     : 0x00060e20  A15     : 0x3ffbfa70  SAR     : 0x00000019  EXCCAUSE: 0x0000001c
     * EXCVADDR: 0x00000000  LBEG    : 0x400014fd  LEND    : 0x4000150d  LCOUNT  : 0xfffffffe
     * 
     * Backtrace: 0x400d3649:0x3ffbf950 0x4012c48a:0x3ffbf970 0x4012d2a5:0x3ffbf990 0x40133552:0x3ffbf9e0 0x40136a05:0x3ffbfa20 0x401
     * 3b4d5:0x3ffbfa40 0x4012c0cb:0x3ffbfa60 0x4008cc61:0x3ffbfa90
     *
     *
     */
    
    /* uint8_t *junk = (uint8_t *) ipaddr; */
    uint8_t junk[] = {0xde, 0xad, 0xbe, 0xef};

    printf("%s(): DNS found the ip address - has type 0x%02x\n", __func__, junk[0]);

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
