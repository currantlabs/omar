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

static void configure_gpio_output(uint8_t gpio)
{
    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };
    gpio_cfg.pin_bit_mask = ((uint64_t)1 << gpio);
    gpio_config(&gpio_cfg);
}

static void configure_gpio_input(uint8_t gpio)
{
    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
    };
    gpio_cfg.pin_bit_mask = ((uint64_t)1 << gpio);
    gpio_config(&gpio_cfg);
}



#if defined(HW_ESP32_PICOKIT)

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



#elif defined(HW_OMAR)

static void gpio_setup(void)
{
    /* Configure outputs */

    configure_gpio_output(OMAR_WHITE_LED0);
    configure_gpio_output(OMAR_WHITE_LED1);

    configure_gpio_output(ADI_RESET);
    gpio_set_level(ADI_RESET, true);    //Don't assert the ADI_RESET just yet:

}


static void button_toggle_state(void)
{
    static bool on = false;

    on = !on;

    // For now, for the hell of it: turn all leds ON, or OFF:
    toggle_white_led0(0, NULL);

}

static void button_toggle_state1(void)
{
    static bool on = false;

    on = !on;

    // For now, for the hell of it: turn all leds ON, or OFF:
    toggle_white_led1(0, NULL);

}

#else

#error No recognized hardware target is #defined!

#endif

static void push_btn_cb(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        button_toggle_state();
    }
}

static void push_btn_cb1(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        button_toggle_state1();
    }
}

static void button_setup(void)
{
    button_handle_t btn_handle = 
        iot_button_create_omar(
            OMAR_SWITCH_INT0,
            BUTTON_ACTIVE_LEVEL);

    if (btn_handle) {
        iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, push_btn_cb, "RELEASE");
    }

    button_handle_t btn_handle1 = 
        iot_button_create_omar(
            OMAR_SWITCH_INT1,
            BUTTON_ACTIVE_LEVEL);

    if (btn_handle1) {
        iot_button_set_evt_cb(btn_handle1, BUTTON_CB_RELEASE, push_btn_cb1, "RELEASE");
    }

}

int toggle_white_led0(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(OMAR_WHITE_LED0, on);

    return 0;
}

int toggle_white_led1(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(OMAR_WHITE_LED1, on);

    return 0;
}

#if defined(HW_ESP32_PICOKIT)

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

#endif //HW_ESP32_PICOKIT
