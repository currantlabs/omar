/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * cmd_omar.c - contains omar-specific console commands
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "esp_console.h"
#include "hw_setup.h"
#include "sdkconfig.h"


static void register_toggle_blue();

void register_omar()
{
    register_toggle_blue();
}

static void register_toggle_blue()
{
    const esp_console_cmd_t cmd = {
        .command = "blue",
        .help = "Toggle the blue LED",
        .hint = NULL,
        .func = &toggle_blue,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

