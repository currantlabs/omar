#include <driver/gpio.h>
#include "gpio_setup.h"

#define BLUE_LED    25
#define GREEN_LED   27
#define RED_LED     26 

void gpio_setup(void)
{
    /* Configure outputs */
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
    };

    io_conf.pin_bit_mask = 
		((uint64_t)1 << BLUE_LED)
		||
		((uint64_t)1 << GREEN_LED)
		||
		((uint64_t)1 << RED_LED);

    /* Configure the GPIO */
    gpio_config(&io_conf);
	
}
