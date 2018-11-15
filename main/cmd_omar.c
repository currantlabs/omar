/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * cmd_omar.c - contains omar-specific console commands
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_console.h"
#include "hw_setup.h"
#include "adi_spi.h"
#include "sdkconfig.h"


#if defined(HW_OMAR)
static void register_toggle_white_led0();
static void register_toggle_white_led1();
#endif


#if defined(HW_ESP32_PICOKIT)
static void register_toggle_blue();
static void register_toggle_green();
static void register_toggle_red();
static void register_toggle();
#endif //HW_ESP32_PICOKIT

#if defined(HW_OMAR)
static void register_hw_detect();
static void register_als();
#endif

static void register_7953();

void register_omar()
{

#if defined(HW_ESP32_PICOKIT)
    register_toggle_blue();
    register_toggle_green();
    register_toggle_red();

    register_toggle();
#endif //HW_ESP32_PICOKIT

    register_7953();

#if defined(HW_OMAR)
    register_toggle_white_led0();
    register_toggle_white_led1();
	register_hw_detect();
	register_als();
#endif

}

#if defined(HW_ESP32_PICOKIT)
// Struct used by the LED toggle function
static struct {
    struct arg_str *color;
    struct arg_end *end;
} toggle_args;


static int toggle(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &toggle_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, toggle_args.end, argv[0]);
        return 1;
    }

    char const *color = toggle_args.color->sval[0];

    ESP_LOGI(__func__, "Toggling the '%s' colored LED",
            color);

    switch (color[0]) {

    case 'r':
        toggle_red(0, NULL);
        break;

    case 'g':
        toggle_green(0, NULL);
        break;

    case 'b':
        toggle_blue(0, NULL);
        break;

    default:

        ESP_LOGI(__func__, "'%s' is not a recognized LED color - please enter either \"red\", \"green\", or \"blue\"",
                 color);

    }


    return 0;
}

static void register_toggle()
{
    toggle_args.color = arg_str0(NULL, NULL, "<red|green|blue>", "LED color to toggle");
    toggle_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "toggle",
        .help = "Toggle the red, green, or blue LEDs",
        .hint = NULL,
        .func = &toggle,
        .argtable = &toggle_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
#endif //HW_ESP32_PICOKIT


#if defined(HW_OMAR)

static void register_toggle_white_led0()
{
    const esp_console_cmd_t cmd = {
        .command = "white_led0",
        .help = "Toggle White LED0",
        .hint = NULL,
        .func = &toggle_white_led0,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_toggle_white_led1()
{
    const esp_console_cmd_t cmd = {
        .command = "white_led1",
        .help = "Toggle White LED1",
        .hint = NULL,
        .func = &toggle_white_led1,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int print_hw_type(int argc, char** argv)
{
	int adc = hw_version_raw();

	printf("ADC reading at the ADC_HW_DET point was %d (0x%02x)\n", 
		   adc, adc);
    return 0;
}

static void register_hw_detect()
{
    const esp_console_cmd_t cmd = {
        .command = "hwdetect",
        .help = "Print out the ADC reading for Hardware Detect",
        .hint = NULL,
        .func = &print_hw_type,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int print_als(int argc, char** argv)
{
	int adc = als_raw();

	printf("Ambient light sensor reading is %d (0x%02x)\n", 
		   adc, adc);
    return 0;
}

static void register_als()
{
    const esp_console_cmd_t cmd = {
        .command = "als",
        .help = "Print out the ADC reading for the Ambient Light Sensor",
        .hint = NULL,
        .func = &print_als,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#endif	// HW_OMAR

#if defined(HW_ESP32_PICOKIT)
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

static void register_toggle_green()
{
    const esp_console_cmd_t cmd = {
        .command = "green",
        .help = "Toggle the green LED",
        .hint = NULL,
        .func = &toggle_green,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static void register_toggle_red()
{
    const esp_console_cmd_t cmd = {
        .command = "red",
        .help = "Toggle the red LED",
        .hint = NULL,
        .func = &toggle_red,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#endif //HW_ESP32_PICOKIT


// Struct used by the LED toggle function
static struct {
    struct arg_str *cmd;
    struct arg_end *end;
} ad7953_args;


static int ad7953(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &ad7953_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ad7953_args.end, argv[0]);
        return 1;
    }

    char const *cmd = ad7953_args.cmd->sval[0];

    if (strcmp(cmd, "hwreset") == 0) {
        adi_hw_reset();
        ESP_LOGI(__func__, "performed a hardware reset");
    } else if (strcmp(cmd, "test") == 0) {
        lcd_get_id();
    } else {
        ESP_LOGI(__func__, "'%s' is not a recognized AD7953 command - please enter either \"hwreset\" or \"swreset\"",
                 cmd);
    }

    return 0;
}

static void register_7953(void)
{
    ad7953_args.cmd = arg_str0(
        NULL, 
        NULL, 
        "<hwreset|test>", 
        "hwreset  -- perform a hardware reset; test -- run factory test");

    ad7953_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "7953",
        .help = "Command the AD7953 to perform some basic operations",
        .hint = NULL,
        .func = &ad7953,
        .argtable = &ad7953_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
