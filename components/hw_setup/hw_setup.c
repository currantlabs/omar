#include <driver/gpio.h>
#include "hw_setup.h"
#include "adi_spi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>

static void gpio_setup(void);
static void button_setup(void);

void omar_setup(void)
{
    gpio_setup();
    button_setup();
    adi_spi_init();
}

static void gpio_setup(void)
{
    /* Configure outputs */

    gpio_config_t io_conf_white_led0 = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_white_led0.pin_bit_mask = ((uint64_t)1 << OMAR_WHITE_LED0);

    /* Configure the GPIO */
    gpio_config(&io_conf_white_led0);
    
    gpio_config_t io_conf_white_led1 = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_white_led1.pin_bit_mask = ((uint64_t)1 << OMAR_WHITE_LED1);

    /* Configure the GPIO */
    gpio_config(&io_conf_white_led1);
    

    gpio_config_t io_conf_blue = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_blue.pin_bit_mask = ((uint64_t)1 << BLUE_LED);

    /* Configure the GPIO */
    gpio_config(&io_conf_blue);
    
    gpio_config_t io_conf_green = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_green.pin_bit_mask = ((uint64_t)1 << GREEN_LED);

    /* Configure the GPIO */
    gpio_config(&io_conf_green);
    
    gpio_config_t io_conf_red = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_red.pin_bit_mask = ((uint64_t)1 << RED_LED);

    /* Configure the GPIO */
    gpio_config(&io_conf_red);
    
    gpio_config_t io_conf_adi_reset = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf_adi_reset.pin_bit_mask = ((uint64_t)1 << ADI_RESET);

    /* Configure the GPIO */
    gpio_config(&io_conf_adi_reset);
    
    //Don't assert the ADI_RESET just yet:
    gpio_set_level(ADI_RESET, true);

}

static void button_toggle_state(void)
{
    static bool on = false;

    on = !on;

    // For now, for the hell of it: turn all leds ON, or OFF:
    gpio_set_level(BLUE_LED, on);
    gpio_set_level(GREEN_LED, on);
    gpio_set_level(RED_LED, on);

}

static void push_btn_cb(void* arg)
{
    static uint64_t previous;
    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        button_toggle_state();
    }
}

static void button_setup(void)
{
    button_handle_t btn_handle = iot_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL);
    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, push_btn_cb, "RELEASE");
    }
}

int toggle_white_led0(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(OMAR_WHITE_LED0, on);

    printf("Just turned white led0 %s\n", (on ? "ON" : "OFF"));

    return 0;
}

int toggle_white_led1(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(OMAR_WHITE_LED1, on);

    printf("Just turned white led1 %s\n", (on ? "ON" : "OFF"));

    return 0;
}

int toggle_blue(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(BLUE_LED, on);

    printf("Just turned the blue led %s\n", (on ? "ON" : "OFF"));

    return 0;
}

int toggle_green(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(GREEN_LED, on);

    printf("Just turned the green led %s\n", (on ? "ON" : "OFF"));

    return 0;
}

int toggle_red(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(RED_LED, on);

    printf("Just turned the red led %s\n", (on ? "ON" : "OFF"));

    return 0;
}

