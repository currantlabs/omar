#include <driver/gpio.h>
#include "hw_setup.h"

#define BLUE_LED    25
#define GREEN_LED   27
#define RED_LED     26 

static void gpio_setup(void);

void omar_setup(void)
{
    gpio_setup();
}

static void  gpio_setup(void)
{
    /* Configure outputs */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf.pin_bit_mask = ((uint64_t)1 << BLUE_LED);

    /* Configure the GPIO */
    gpio_config(&io_conf);
    
}

int toggle_blue(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    gpio_set_level(BLUE_LED, on);

    printf("Just turned the blue led %s\n", (on ? "ON" : "OFF"));

    return 0;
}

