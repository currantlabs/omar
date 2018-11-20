/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * powertest.c - code to run continuously, making omar burn as much power as possible
 */

#if defined(POWERTEST)


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"

// Omar headers:
#include "hw_setup.h"
#include "cmd_decl.h"


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
        printf("powertest started:\n");

    initialize_nvs();
    omar_setup();
    initialise_wifi();


        while (1){
                printf("powertesting...\n");
                vTaskDelay(1000/portTICK_PERIOD_MS);
        }

}

#endif //defined(POWERTEST)
