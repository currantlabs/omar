/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * cmd_omar.c - contains omar-specific console commands
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "argtable3/argtable3.h"
#define MAX_ARGTABLE3_STRING_ARG_LENGTH             (224)

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_console.h"
#include "hw_setup.h"
#include "omar_als_timer.h"
#include "adi_spi.h"
#include "sdkconfig.h"
#if defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)
#include "s5852a.h"
#endif //defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)

#if defined(HW_OMAR)
#include "s24c08.h"
#endif //defined(HW_OMAR) 

static void register_version_info();

#if defined(HW_OMAR)
static void register_toggle_white_led0();
static void register_toggle_white_led1();
#endif


#if defined(HW_ESP32_PICOKIT)
static void register_toggle_blue();
static void register_toggle_green();
static void register_toggle_red();
static void register_toggle();
static void register_temperature();
#endif //HW_ESP32_PICOKIT

#if defined(HW_OMAR)
static void register_hw_detect();
static void register_als();
static void register_temperature();
static void register_eeprom();
static void register_ledpwm();
#endif

static void register_7953();

void register_omar()
{

    register_version_info();

#if defined(HW_ESP32_PICOKIT)
    register_toggle_blue();
    register_toggle_green();
    register_toggle_red();

    register_toggle();
    register_temperature();
#endif //HW_ESP32_PICOKIT

    register_7953();

#if defined(HW_OMAR)
    register_toggle_white_led0();
    register_toggle_white_led1();
    register_hw_detect();
    register_als();
    register_temperature();
    register_eeprom();
    register_ledpwm();
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

#if defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)
static int print_temp(int argc, char** argv)
{
    float temp; 

    s5852a_get(&temp);

    printf("Current temperature is %02.2f\n", temp);
    return 0;
}

static void register_temperature()
{
    const esp_console_cmd_t cmd = {
        .command = "temp",
        .help = "Print out the current temperature reading from the S-5852A temp sensor",
        .hint = NULL,
        .func = &print_temp,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
#endif //defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)




#if defined(HW_OMAR)

static struct {
    struct arg_lit *write;      // write
    struct arg_lit *read;       // read 
    struct arg_lit *dump;       // dump all 1024 bytes of eeprom memory
    struct arg_lit *erase;      // set all 1024 bytes of eeprom memory to 0xff
    struct arg_lit *blast;      // set all 1024 bytes of eeprom memory to the specified value
    struct arg_int *address;    // Ranges from 0x000 to 0x400
    struct arg_int *count;      // number of bytes to read 
    struct arg_int *value;      // (only for "write") Value to be written
    struct arg_str *values;     // a string of hex values to be written
    struct arg_end *end;
} eeprom_args;

static bool valid_hexadecimal_value(const char *arg)
{
    int length = strlen(arg);

    if (length % 2 != 0) {
        printf("%s(): a properly-formed hex argument must contain an even number of digits (i.e., you must write \"0xf\" as \"0f\" etc)\n", __func__);
        return false;
    }
                            
    // Too bad esp-idf doesn't support the "regex.h" part of newlib yet!
    // Have to do some stuff by hand..
    if (arg[0] == '0' && arg[1] == 'x') {
        printf("%s(): a leading \"0x\" is not required at the start of the hexadecimal digit as is done in [%s]\n", __func__, arg);
        return false;
    }
    
    // Check for valid digits:
    for (int i=0; i<length; i++) {
        char digit = arg[i];

        if (digit >= '0' && digit <= '9')
            continue;

        if (digit >= 'a' && digit <= 'f')
            continue;

        if (digit >= 'A' && digit <= 'F')
            continue;

        printf("%s(): the digit \"%c\" is not part of a hexadecimal number so [%s] isn't valid\n", __func__, digit, arg);
        return false;

    }
    
    // Everying checks out!
    return true;

}

static uint8_t eeprom_read_buf[OMAR_EEPROM_SIZE] = {0}; // For debug purposes for the moment

static int access_eeprom(int argc, char** argv)
{
    static int address = 0;
    uint16_t count;
    char operation; 

    /*
     * The 'value' and 'values' variables only come into play
     * during a write operation.
     *
     * 'value' is used to specify a particular byte that is
     * written some number of times (typically once, but the
     * user can specify that it be repeated using the --count
     * option).
     *
     * 'values' points to a string of hexadecimal digits, and
     * holds the data indicated when the --values option is used.
     *
     */
    uint8_t value = 0xff;
    const char *values;

    /*
     * A 'repeated_write' is writing the same byte of data the 
     * specified (by --count) number of times .
     * 
     * A 'multiple_write' means you've specified a string of
     * hexadecimal values to be written (using the --values option).
     *
     * These are mutually exclusive!
     */
    bool repeated_write = false;
    bool multiple_write = false;

    int nerrors = arg_parse(argc, argv, (void**) &eeprom_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, eeprom_args.end, argv[0]);
        return 1;
    }

    // (0) First check for special cases like 'dump' or 'erase':
    if (eeprom_args.dump->count == 1) {

        uint8_t *buf = eeprom_read_buf;
        count = OMAR_EEPROM_SIZE;
        address = 0;

#if defined(MEASURE_EEPROM_WRITE_TIME)
        int startTime, finishTime;
        startTime = xTaskGetTickCount();
#endif

        esp_err_t ret = s24c08_read((uint16_t )0, buf, count);

        if (ret != ESP_OK) {
            printf("%s(): s24c08_read() call returned an error - 0x%x\n", __func__, ret);
            return 1;
        }

#if defined(MEASURE_EEPROM_WRITE_TIME)
        finishTime = xTaskGetTickCount();
        printf("%s(): Reading all 1024 bytes of eeprom took %d ticks (that's about %d milliseconds, since 1 tick is 10 milliseconds)\n", 
               __func__, 
               finishTime - startTime,
               (finishTime - startTime) * 10);
#endif

        for (int i=0; i<=count/16; i++) {
            if (i*16 == count) {
                break;
            }
            printf("0x%04x: ", address + i*16);
            for (int j=0; i*16+j < count && j < 16; j++) {
                printf("0x%02x ", buf[i*16+j]);
            }
            printf("\n");
        }


        goto finish;
    }


    if (eeprom_args.erase->count == 1 && eeprom_args.blast->count == 1) {
        printf("%s(): cannot specify both an \"erase\" and a \"blast\" operation - pick one\n", __func__);
        return 1;
    }

    if (eeprom_args.erase->count == 1 || eeprom_args.blast->count == 1) {
        uint8_t *buf = eeprom_read_buf;
        uint8_t write_value = 0;
        count = OMAR_EEPROM_SIZE;
        address = 0;

        if (eeprom_args.blast->count == 1 && eeprom_args.value->count == 0) {
            printf("%s(): you must specify a \"value\" to perform a \"blast\" operation\n", __func__);
            return 1;
        }

        if (eeprom_args.blast->count == 1) {
            write_value = eeprom_args.value->ival[0];
        }

        memset(buf, write_value, OMAR_EEPROM_SIZE);

#if defined(MEASURE_EEPROM_WRITE_TIME)
        int startTime, finishTime;
        startTime = xTaskGetTickCount();
#endif

        esp_err_t ret = s24c08_write(0, buf, count);

        if (ret != ESP_OK) {
            printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret);
            return 1;
        }

#if defined(MEASURE_EEPROM_WRITE_TIME)
        finishTime = xTaskGetTickCount();
        printf("%s(): Erasing all 1024 bytes of eeprom took %d ticks (that's about %d milliseconds, since 1 tick is 10 milliseconds)\n", 
               __func__, 
               finishTime - startTime,
               (finishTime - startTime) * 10);
#endif

        goto finish;
    }

    if (eeprom_args.erase->count == 1) {
        uint8_t *buf = eeprom_read_buf;
        count = 16;
        address = 0;
        
        for (int i=0; i<count; i++) {
            buf[i]= 0xff;
        }

        for (int i=0; i<(OMAR_EEPROM_SIZE >> 4); i++) {

            esp_err_t ret = s24c08_write(address + i*count, buf, count);
            if (ret != ESP_OK) {
                printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret);
                return 1;
            }

        }

        goto finish;

    }

    // (1) Figure out whether it's a 'read' or a 'write':
    if (eeprom_args.read->count == 0
        &&
        eeprom_args.write->count == 0) {
        printf("%s(): specify either a \"read\" or a \"write\" operation\n", __func__);
        return 1;
    }

    if (eeprom_args.read->count == 1
        &&
        eeprom_args.write->count == 1) {
        printf("%s(): cannot specify both a \"read\" and a \"write\" operation - pick one\n", __func__);
        return 1;
    }

    operation = (eeprom_args.read->count == 1 ? 'r' : 'w');

    // Specifying either --value or --values makes no sense if you're reading:
    if (operation == 'r'
        &&
        (eeprom_args.value->count == 1 || eeprom_args.values->count == 1)) {

        printf("%s(): the --value and --values options only apply to write operations\n", __func__);
        return 1;

    }
        

    // (2) Get the address (or default to last address if none specified):
    if (eeprom_args.address->count != 0) {
        int addr = eeprom_args.address->ival[0];
        if (addr < 0 || addr > OMAR_EEPROM_MAXADDR) {
            printf("%s() invalid address - please specify something between 0x%03x and 0x%03x\n", 
                   __func__, 
                   0,
                   OMAR_EEPROM_MAXADDR);

            return 1;

        } else {
            address = addr;
        }
    
    }

    // (3) Find out what the count is (or default to 1 if not specified):
    if (eeprom_args.count->count != 0) {
        count = eeprom_args.count->ival[0];

        if (operation == 'w' && count > 1) {
            repeated_write = true;
        }

    } else {
        // If --count is not specified, default to a count of 1:
        count = 1;
    }

    // (4) If this is a write operation, figure out what we're writing
    if (operation == 'w') {

        if (eeprom_args.value->count == 1
            &&
            eeprom_args.values->count == 1) {
            printf("%s(): the \"--value\" and \"--values\" options are mutually exclusive - pick one\n", __func__);
            return 1;
        }

        // Check for a very specific corner case: specifying --count and
        // --values at the same time, which isn't allowed:
        if (eeprom_args.count->count != 0
            &&
            eeprom_args.values->count != 0) {

            printf("%s(): you cannot specify \"--count\" and \"--values\" at the same time (operation not supported)\n", __func__);
            return 1;
        }

        if (eeprom_args.value->count == 0
            &&
            eeprom_args.values->count == 0) {

            // If a value isn't specified, default to 0xff
            value = 0xff;

            // Check for the special "short-cut" case where
            // you can specify repeated writes of a given value
            if (count > 1) {
                repeated_write = true;
            }

        } else if (eeprom_args.values->count != 0) {
            values = eeprom_args.values->sval[0];

            if (!valid_hexadecimal_value(values)) {
                printf("%s(): the specified string of hexadecimal values isn't valid\n", __func__);
                return 1;
            }

            multiple_write = true;

            // There is apparently a limit in the 'argtable3' framework
            // to the length of string arguments: anything bigger than 224
            // bytes in length will be corrupt, so check for that first:
            if (strlen(values) > MAX_ARGTABLE3_STRING_ARG_LENGTH) {
                printf("%s(): our argument parsing framework, argtable3, chokes on string args longer than %d bytes (you specified one %d bytes long)\n",
                       __func__,
                       MAX_ARGTABLE3_STRING_ARG_LENGTH,
                       strlen(values));
                return 1;
            }
            
            count = (strlen(values) >> 1); // the 'values' string is composed of 'nibbles' and two nibbles = 1 byte
        } else {
            value = eeprom_args.value->ival[0];
        }
            
        // Final consistency check:
        if (repeated_write && multiple_write) {
            printf("%s(): argument parsing error (we think we're doing both a repeated_write and a multiple_write)", __func__);
            return 1;
        }

    }

    // (6) Alert the user if they're trying to read/write past the end of eeprom
    if (address + count > OMAR_EEPROM_MAXADDR+1) {
        printf("%s(): you are attempting to %s past the end of eeprom memory at 0x%03x (addr = 0x%03x + 0x%03x [%d] > 0x%03x)\n",
               __func__,
               (operation == 'r' ? "read" : "write" ),
               OMAR_EEPROM_MAXADDR,
               address,
               count, count,
               OMAR_EEPROM_MAXADDR);

        return 1;

    }


    // (6) Summarize what we're doing:
    if (operation == 'r') {
        printf("%s(): eeprom read of %d bytes starting from address 0x%04x\n",
               __func__,
               count,
               address);

        uint8_t *buf = eeprom_read_buf;

        esp_err_t ret = s24c08_read((uint16_t )address, buf, count);
        if (ret != ESP_OK) {
            printf("%s(): s24c08_read() call returned an error - 0x%x\n", __func__, ret);
            return 1;
        }

        if (count == 1) {
            printf("%s(): read 0x%02x from eeprom location 0x%04x\n", __func__, buf[0], address);
        } else {
            for (int i=0; i<=count/16; i++) {
                if (i*16 == count) {
                    break;
                }
                printf("0x%04x: ", address + i*16);
                for (int j=0; i*16+j < count && j < 16; j++) {
                    printf("0x%02x ", buf[i*16+j]);
                }
                printf("\n");
            }
        }
    } else {
        if (multiple_write) {

            printf("%s(): eeprom multiple write of the %d bytes comprising the hexadecimal string [0x%s], starting from address 0x%04x\n",
                   __func__,
                   count,
                   values,
                   address);

            uint8_t *buf = eeprom_read_buf;
            for (int i=0; i<count; i++) {
                const char digit[] = {values[2*i], values[2*i+1], 0};

                if (digit[0] == '0' && digit[1] == '0') {
                    // strtoul() returns 0 if no conversion could
                    // be performed
                    buf[i] = 0;
                    continue;
                }

                unsigned long value = strtoul(digit, NULL, 16);
                if (value == 0) {
                    printf("%s(): strtoul() failed to convert [%s] to an integer\n", __func__, digit);
                    return 1;
                }

                buf[i] = (uint8_t ) value;

            }

            esp_err_t ret = s24c08_write(address, buf, count);
            if (ret != ESP_OK) {
                printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret);
                return 1;
            }



        } else if (repeated_write) {
            
            printf("%s(): eeprom repeated write of the value 0x%02x, %d times, starting from address 0x%04x\n",
                   __func__,
                   value,
                   count,
                   address);

            // Set up the transfer buffer:
            uint8_t *buf = eeprom_read_buf;
            for (int i=0; i<count; i++) {
                buf[i] = value;
            }

            esp_err_t ret = s24c08_write(address, buf, count);
            if (ret != ESP_OK) {
                printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret);
                return 1;
            }


        } else {
            printf("%s(): eeprom write of the value 0x%02x to address 0x%04x\n",
                   __func__,
                   value,
                   address);

            esp_err_t ret = s24c08_write(address, &value, 1);
            if (ret != ESP_OK) {
                printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret);
                return 1;
            }
        }
    }

finish: 

    return 0;

}

static void register_eeprom()
{
    eeprom_args.read = arg_lit0(
        "r", 
        "read", 
        "Read data from eeprom");

    eeprom_args.write = arg_lit0(
        "w", 
        "write", 
        "Write data to eeprom");

    eeprom_args.dump = arg_lit0(
        "d", 
        "dump", 
        "Dump all 1024 bytes of eeprom memory");

    eeprom_args.erase = arg_lit0(
        "e", 
        "erase", 
        "Set all 1024 bytes of eeprom memory to 0x00");

    eeprom_args.blast = arg_lit0(
        "b", 
        "blast", 
        "Set all 1024 bytes of eeprom memory to --value");

    eeprom_args.count = arg_int0(
        "c",
        "count", 
        "<int>", 
        "Number of bytes to read or write (when writing, number of times to write a value)");

    eeprom_args.address = arg_int0(
        "a", 
        "address", 
        "<int>", 
        "Address to access, 0x000 - 0x3ff (defaults to 0, or the last address specified)");

    eeprom_args.value = arg_int0(
        "v",
        "value", 
        "<int>", 
        "Value to be written (defaults to 0xff)");

    eeprom_args.values = arg_str0(
        "V", 
        "values", 
        "<string>", 
        "A string of hexadecimal digits to be written");

    eeprom_args.end = arg_end(8);

    const esp_console_cmd_t eeprom_cmd = {
        .command = "eeprom",
        .help = "Write data to, or read data from the s24c08 eeprom",
        .hint = NULL,
        .func = &access_eeprom,
        .argtable = &eeprom_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&eeprom_cmd) );
}

static struct {
    struct arg_int *led;        // specify the led to dim/brighten - "1" or "2"
    struct arg_lit *getduty;    // return the current duty cycle in effect for the specified led
    struct arg_int *setduty;    // set the pwm duty cycle for the led (maximum is OMAR_LED_MAX_DUTY or 8191)
    struct arg_int *brighten;   // increase the pwm duty cycle by the specified amount
    struct arg_int *dim;        // decrease the pwm duty cycle by the specified amount
    struct arg_end *end;
} ledpwm_args;


static int led_pwm(int argc, char** argv)
{

    /* uint32_t duty_cyle_delta = 100; */

    int nerrors = arg_parse(argc, argv, (void**) &ledpwm_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, ledpwm_args.end, argv[0]);
        return 1;
    }

    if (ledpwm_args.brighten->count == 1
        &&
        ledpwm_args.dim->count == 1) {
        printf("%s(): the \"--brighten\" and \"--dim\" options are mutually exclusive - pick one\n", __func__);
        return 1;
    }

    if (ledpwm_args.getduty->count == 1
        &&
        ledpwm_args.setduty->count == 1) {
        printf("%s(): the \"--get\" and \"--set\" options are mutually exclusive - pick one\n", __func__);
        return 1;
    }

    bool brightdim = 
      (ledpwm_args.brighten->count == 1)
      ||
      (ledpwm_args.dim->count == 1);

    bool getset = 
      (ledpwm_args.getduty->count == 1)
      ||
      (ledpwm_args.setduty->count == 1);

    if (brightdim && getset) {
        printf("%s(): the \"--get\"/\"--set\" apis, and the \"--brighten\"/\"--dim\" apis are mutually exclusive - pick one\n", __func__);
        return 1;
    }

    int led = ledpwm_args.led->ival[0];

    if (!( led == 1 || led == 2)) {
        printf("%s(): the \"--led\" must equal either 1 or 2\n", __func__);
        return 1;
    }

    if (!(brightdim || getset)) {
        printf("%s(): Pick something you want to do -- either \"--get\", \"--set\", \"--brighten\" or \"--dim\"\n", __func__);
        return 1;
    }

    char operation; 
    if (brightdim) {
        operation = (ledpwm_args.brighten->count == 0 ? 'd' : 'b');
    } else if (getset) {
        operation = (ledpwm_args.getduty->count == 0 ? 's' : 'g');
    }

    uint8_t led_gpio = (led == 1 ? OMAR_WHITE_LED0 : OMAR_WHITE_LED1);
    uint32_t new_duty_cycle = led_get_brightness(led_gpio);

    if (brightdim) {
        if (operation == 'd') {
            new_duty_cycle -= ledpwm_args.dim->ival[0];
        } else {
            new_duty_cycle += ledpwm_args.brighten->ival[0];
        }

        led_set_brightness(led_gpio, new_duty_cycle);

        printf("%s(): %s's new duty cycle is %d\n",
               __func__,
               (led == 1 ? "OMAR_WHITE_LED0" : "OMAR_WHITE_LED1"),
               led_get_brightness(led_gpio));

    } else if (getset) {
        if (operation == 'g') {
            printf("%s(): %s's current duty cycle is %d\n",
                   __func__,
                   (led == 1 ? "OMAR_WHITE_LED0" : "OMAR_WHITE_LED1"),
                   led_get_brightness(led_gpio));

        } else if (operation == 's') {
            new_duty_cycle = ledpwm_args.setduty->ival[0];
            led_set_brightness(led_gpio, new_duty_cycle);

        }

    } else {
        printf("%s(): No operation specified - pick something you want to do -- either \"--get\", \"--set\", \"--brighten\" or \"--dim\"\n", __func__);
        return 1;
    }

    return 0;
}

static void register_ledpwm()
{
    ledpwm_args.led = arg_int1(
        "l", 
        "led", 
        "<int>", 
        "Specify led 1 or 2");

    ledpwm_args.getduty = arg_lit0(
        "g", 
        "get", 
        "Returns the pwm duty cycle for the specified led");

    ledpwm_args.setduty = arg_int0(
        "s", 
        "set", 
        "<int>", 
        "Specify the pwm duty cycle for the led (min is 0, max is 8191)");

    ledpwm_args.brighten = arg_int0(
        "b", 
        "brighten", 
        "<int>", 
        "Brighten the led by increasing the pwm duty cycle by the specified amount");


    ledpwm_args.dim = arg_int0(
        "d", 
        "dim", 
        "<int>", 
        "Dim the led by decreasing the pwm duty cycle by the specified amount");

    ledpwm_args.end = arg_end(3);

    const esp_console_cmd_t pwm_cmd = {
        .command = "pwm",
        .help = "Dim or brighten the leds; get/set the current pwm duty cycle for each led",
        .hint = NULL,
        .func = &led_pwm,
        .argtable = &ledpwm_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&pwm_cmd) );
}

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

static struct {
    struct arg_lit *timer_off;
    struct arg_lit *timer_on;
    struct arg_lit *display_periods;
    struct arg_int *secondarytimer_period;  // in microseconds
    struct arg_lit *capture; // Sample for 60Hz noise for a period of seconds
    struct arg_lit *report; // Dump the contents of memory, showing samples
    struct arg_lit *export; // Print the array of data as a single column of 
                            // decimal numbers (handy for exporting to Excel 
                            // for plotting)
    struct arg_end *end;
} als_args;

static int print_als(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &als_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, als_args.end, argv[0]);
        return 1;
    }

    // Handle the als "capture/report/export" cases first:
    if (als_args.capture->count != 0) {

        start_als_sample_capture();
        return 0;
    }

    // Handle the als "capture/report/export" cases first:
    if (als_args.report->count != 0) {

        report_als_samples(HEXDUMP_REPORT_FORMAT);
        return 0;
    }


    // Handle the als "capture/report/export" cases first:
    if (als_args.export->count != 0) {

        report_als_samples(SINGLECOLUMNDECIMAL_REPORT_FORMAT);
        return 0;
    }


    // Take care of the simpler "dump timer periods" case first:
    if (als_args.display_periods->count != 0) {

        if (als_args.timer_off->count != 0
            ||
            als_args.timer_on->count != 0) {

            printf("%s(): The \"--enable\" and \"--disable\" options aren't allowed when calling \"--gettimerperiods\"\n", __func__);
               
            return 1;
        }
        
        printf("Ambient light sensor timer periods\r\n\tPrimary:\t%0.2f (seconds)\r\n\tSecondary:\t%0.2f (microseconds)\n",
               get_als_timer_period(PRIMARY_TIMER),
               1000000 * get_als_timer_period(SECONDARY_TIMER));


        return 0;
    }

    // If you're not enabling/disabling/setting anything
    // just dump the current als value:
    if (als_args.timer_off->count == 0
        &&
        als_args.timer_on->count == 0
        &&
        als_args.secondarytimer_period->count == 0) {
        int adc = als_raw();

        printf("Ambient light sensor reading is %d (0x%02x)\n", 
               adc, adc);
        return 0;
    }

    // Next handl the case of setting either of the ambient
    // light sensor timer periods:
    if (als_args.secondarytimer_period->count != 0) {

        if (als_args.timer_off->count != 0
            ||
            als_args.timer_on->count != 0) {

            printf("%s(): The \"--enable\" and \"--disable\" options aren't allowed when setting either of the ambient light sensor periods\n", __func__);
               
            return 1;
        }
        
        // Because the primary timer kicks off the secondary timer,
        // its period must be greater than that of the secondary timer.
        // So check this first...

        double secondarytimerperiod = 
            (als_args.secondarytimer_period->count != 0 
             ? 
             (double ) (als_args.secondarytimer_period->ival[0]/1000000.0)
             :
             get_als_timer_period(SECONDARY_TIMER));


        double primarytimerperiod = get_als_timer_period(PRIMARY_TIMER);

        if (secondarytimerperiod >= primarytimerperiod/2) {
            printf("%s(): Error/Abort - the primary als timer period (%.8f seconds) must be at least twice as long as the secondary period (%.8f seconds)\n",
                   __func__,
                   primarytimerperiod,
                   secondarytimerperiod);

            return 1;
        }

        if (als_args.secondarytimer_period->count != 0) {
            set_als_timer_period(SECONDARY_TIMER, ((double ) als_args.secondarytimer_period->ival[0])/1000000.0);
        } 
        

        return 0;

    }   

    // You can't disable and enable at the same time:
    if (als_args.timer_off->count != 0
        &&
        als_args.timer_on->count != 0) {

        printf("%s(): The \"--enable\" and \"--disable\" options are mutually exculsive - pick one\n", __func__);
               
        return 1;
    }
    
    enable_als_timer(als_args.timer_on->count != 0);

    return 0;

}

static void register_als()
{
    als_args.timer_off = arg_lit0(
        "d", 
        "disable", 
        "Disables the timer that triggers automatic als sampling");

    als_args.timer_on = arg_lit0(
        "e", 
        "enable", 
        "Enables the timer that triggers automatic als sampling");

    als_args.display_periods = arg_lit0(
        "g", 
        "gettimerperiods", 
        "Show the als timer periods, both primary (seconds) and secondary (microseconds)");

    als_args.secondarytimer_period = arg_int0(
        "s", 
        "secondary", 
        "<int>", 
        "Set the secondary als timer period (microseconds)");
    
    als_args.capture = arg_lit0(
        "c", 
        "capture", 
        "Capture 2 seconds worth of ambient light sensor (als) data");

    als_args.report = arg_lit0(
        "r", 
        "report", 
        "Display 2 seconds worth of ambient light sensor (als) data");

    als_args.export = arg_lit0(
        "x", 
        "export", 
        "Display 2 seconds worth of ambient light sensor data as a single column of decimal numbers");

    als_args.end = arg_end(5);

    const esp_console_cmd_t cmd = {
        .command = "als",
        .help = "Print out the ADC reading for the Ambient Light Sensor",
        .hint = NULL,
        .func = &print_als,
        .argtable = &als_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#endif  // HW_OMAR

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
        printf("OLDE WAY\r\n");
        lcd_get_id(); 
        vTaskDelay(10/portTICK_PERIOD_MS);
        printf("NEW WAY\r\n");

        uint8_t rxbuf[8];
        uint8_t rxlen = spi_read_reg(CONFIG, rxbuf);

        if (rxlen > 5) {
            printf("%s(): spi_read_reg() returned bad rxlen of %d\n", __func__, rxlen);
        } else {
            printf("\r\n");
            for (int i=0; i<rxlen; i++) {
                printf("0x%02x ", rxbuf[i]);
            }
            printf("\r\n");
        }

        vTaskDelay(10/portTICK_PERIOD_MS);

        // Next, read the value of the AWGAIN register:
        rxlen = spi_read_reg(AWGAIN, rxbuf);
        if (rxlen > 5) {
            printf("%s(): spi_read_reg() returned bad rxlen of %d\n", __func__, rxlen);
        } else {
            printf("\r\n");
            printf("AWGAIN register: ");
            for (int i=0; i<rxlen; i++) {
                printf("0x%02x ", rxbuf[i]);
            }
            printf("\r\n");
        }

        vTaskDelay(10/portTICK_PERIOD_MS);

        // Now, write something to AWGAIN
        uint8_t txbuf[] = {0x1a, 0xdd, 0xad};
        printf("Writing {0x%02x, 0x%02x, 0x%02x} to AWGAIN register...\n",
               txbuf[0], txbuf[1], txbuf[2]);
        spi_write_reg(AWGAIN, txbuf);

        vTaskDelay(10/portTICK_PERIOD_MS);

        // And finally, read it out again:
        rxlen = spi_read_reg(AWGAIN, rxbuf);
        if (rxlen > 5) {
            printf("%s(): spi_read_reg() returned bad rxlen of %d\n", __func__, rxlen);
        } else {
            printf("\r\n");
            printf("AWGAIN register: ");
            for (int i=0; i<rxlen; i++) {
                printf("0x%02x ", rxbuf[i]);
            }
            printf("\r\n");
        }




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

static int omar_version(int argc, char** argv)
{
    printf("Verion %s on branch \"%s\", built on %s\n", OMAR_VERSION, OMAR_BRANCH, OMAR_TIMESTAMP);
    return 0;
}

static void register_version_info()
{
    const esp_console_cmd_t cmd = {
        .command = "version",
        .help = "Version and build info of this Omar FW",
        .hint = NULL,
        .func = &omar_version,
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

