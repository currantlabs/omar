/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * cmd_omar.c - contains omar-specific console commands
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_console.h"
#include "hw_setup.h"
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
    struct arg_int *address;    // Ranges from 0x000 to 0x400
    struct arg_int *count;      // number of bytes to read 
    struct arg_int *value;      // (only for "write") Value to be written
    struct arg_str *values;     // a string of hex values to be written
    struct arg_end *end;
} eeprom_args;

static void restore_eeprom_command_option_defaults(void)
{
    /* eeprom_args.count->ival[0] = 1; */
    /* eeprom_args.address->ival[0] = 0; */
    /* eeprom_args.value->ival[0] = 0xff; */
    /* eeprom_args.values->sval[0] = "ff"; */
}

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

static int access_eeprom(int argc, char** argv)
{
    static int address = 0;
    uint16_t count;
    /* static uint8_t write_buffer[OMAR_EEPROM_PAGE_SIZE] = {0}; */
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
        restore_eeprom_command_option_defaults();
        return 1;
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

        // Alert the user if they're trying to read/write past the end of eeprom
        if (address + count > OMAR_EEPROM_MAXADDR) {
            printf("%s(): you are attempting to %s past the end of eeprom memory at 0x%03x (addr = 0x%03x + 0x%03x [%d] > 0x%03x)\n",
                   __func__,
                   (operation == 'r' ? "read" : "write" ),
                   OMAR_EEPROM_MAXADDR,
                   address,
                   count, count,
                   OMAR_EEPROM_MAXADDR);

            return 1;

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
            count = strlen(values);
        } else {
            value = eeprom_args.value->ival[0];
        }
            
        // Final consistency check:
        if (repeated_write && multiple_write) {
            printf("%s(): argument parsing error (we think we're doing both a repeated_write and a multiple_write)", __func__);
            return 1;
        }

    }


    // (5) Summarize what we're doing:
    if (operation == 'r') {
        printf("%s(): eeprom read of %d bytes starting from address 0x%04x\n",
               __func__,
               count,
               address);
    } else {
        if (multiple_write) {

            printf("%s(): eeprom multiple write of the %d bytes comprising the hexadecimal string [0x%s], starting from address 0x%04x\n",
                   __func__,
                   count,
                   values,
                   address);

        } else if (repeated_write) {
            
            printf("%s(): eeprom repeated write of the value 0x%02x, %d times, starting from address 0x%04x\n",
                   __func__,
                   value,
                   count,
                   address);

        } else {
            printf("%s(): eeprom write of the value 0x%02x to address 0x%04x\n",
                   __func__,
                   value,
                   address);
        }
    }


    return 0;

    /* const char op = eeprom_args.operation->sval[0][0]; */

    /* if (!(op == 'w' || op == 'r')) { */
    /*     printf("eeprom: invalid operation \'%c\' (must be 'r' or 'w')\n", op); */
    /*     restore_eeprom_command_option_defaults(); */
    /*     return 1; */
    /* } */

    /* const int value_specified = eeprom_args.value->count; */
    /* if (value_specified && op == 'r') { */
    /*     printf("eeprom: don't specify a value when performing a read operation\n"); */
    /*     restore_eeprom_command_option_defaults(); */
    /*     return 1; */
    /* } */

    /* const int address_specified = eeprom_args.address->count; */
    
    /* if (!address_specified) { */
    /*     default_address++; */
    /* } else { */
    /*     // Check to see if the address is out of bounds: */
    /*     int address = eeprom_args.address->ival[0]; */

    /*     if (address < 0 || address >= OMAR_EEPROM_SIZE) { */
    /*         printf("eeprom: address 0x%03x out of range (must be between 0x000 and 0x3ff)\n", address); */
    /*         restore_eeprom_command_option_defaults(); */
    /*         return 1; */
    /*     } */

    /*     // Remember this address for next time */
    /*     default_address = eeprom_args.address->ival[0]; */
    /* } */


    /* printf("%s(): Operation = [%s], count = %d, address = 0x%03x, and value = 0x%02x\n", */
    /*        __func__, */
    /*        eeprom_args.operation->sval[0], */
    /*        eeprom_args.count->ival[0], */
    /*        default_address, */
    /*        eeprom_args.value->ival[0]); */


    /* uint8_t value = eeprom_args.value->ival[0]; */

    /* int count = eeprom_args.count->ival[0]; */

    /* bool multiple_write_values_specified = eeprom_args.values->count != 0; */
    /* const char *write_values = NULL; */

    /* if (multiple_write_values_specified) { */
    /*     write_values = eeprom_args.values->sval[0]; */
    /* } */

    /* restore_eeprom_command_option_defaults(); */

    /* if (op == 'r' && multiple_write_values_specified) { */
    /*     printf("%s(): you cannot specify multiply write values when performing a read operation\n", __func__); */
    /*     return 1; */
    /* } */

    /* if (op == 'r'  */
    /*     &&  */
    /*     (count + (default_address % OMAR_EEPROM_PAGE_SIZE)) > OMAR_EEPROM_PAGE_SIZE) { */

    /*     printf("%s(): can't read past the edge of a page of eeprom memory (base address = 0x%03x, count = 0x%02x)\n", */
    /*            __func__, default_address, count); */

    /*     return 1; */

    /* } */

    /* if (op == 'r') { */
    /*     uint8_t data[16] = {0}; */
    /*     uint8_t *buf = (count <= 16 ? data : calloc(count, 1)); */
    /*     int retval = 0;  */

    /*     esp_err_t ret = s24c08_read((uint16_t )default_address, buf, count); */
    /*     if (ret != ESP_OK) { */
    /*         printf("%s(): s24c08_read() call returned an error - 0x%x\n", __func__, ret); */
    /*         retval = 1; */
    /*     } */

    /*     if (count == 1) { */
    /*         printf("%s(): read 0x%02x from eeprom location 0x%x\n", __func__, buf[0], default_address); */
    /*     } else { */
    /*         for (int i=0; i<count/16; i++) { */
    /*             printf("0x%04x: ", default_address + i*16); */
    /*             for (int j=0; i*16+j < count && j < 16; j++) { */
    /*                 printf("0x%02x ", buf[i*16+j]); */
    /*             } */
    /*             printf("\n"); */
    /*         } */

            
    /*     } */

    /*     // Clean up if you have to */
    /*     if (buf != data) { */
    /*         free(buf); */
    /*     } */

    /*     return retval; */

    /* } */

    /* // It's a write operation: */

    /* if (op == 'w' && !address_specified) { */
    /*     printf("%s(): must specify the address when writing data\n", __func__); */
    /*     return 0; */
    /* } */

    /* if (multiple_write_values_specified) { */

    /*     if (!valid_hexadecimal_value(write_values)) { */
    /*         return 1; */
    /*     } */

    /*     uint16_t xfer_size = strlen(write_values)/2; */

    /*     if (xfer_size > OMAR_EEPROM_PAGE_SIZE) { */
    /*         printf("%s(): attempting to write %d bytes, more than the max xfer of %d bytes\n",  */
    /*                __func__,  */
    /*                xfer_size,  */
    /*                OMAR_EEPROM_PAGE_SIZE); */

    /*         return 1; */
    /*     } */


    /*     printf("%s(): attempting to write multiple values to a location: [%s]\n", __func__, write_values); */

    /*     // Convert the string of hex digits into actual hex values: */
    /*     for (int i=0; i<xfer_size; i++) { */
    /*         const char digit[] = {write_values[2*i], write_values[2*i+1]}; */

    /*         if (digit[0] == '0' && digit[1] == '0') { */
    /*             // strtoul() returns 0 if no conversion could */
    /*             // be performed */
    /*             write_buffer[i] = 0; */
    /*             continue; */
    /*         } */

    /*         unsigned long value = strtoul(digit, NULL, 16); */
    /*         if (value == 0) { */
    /*             printf("%s(): strtoul() failed to convert [%s] to an integer\n", __func__, digit); */
    /*             return 1; */
    /*         } */

    /*         write_buffer[i] = (uint8_t ) value; */

    /*     } */
        

    /*     printf("%s(): calling s24c08_write(0x%03x, 0x%02x, %d)\n", */
    /*            __func__, default_address, write_buffer[0], xfer_size); */

    /*     if (s24c08_write(default_address, write_buffer, xfer_size) != ESP_OK) { */
    /*         return 1; */
    /*     } */
        
    /* } else { */

    /*     esp_err_t ret = s24c08_write(default_address, &value, 1); */
    /*     if (ret != ESP_OK) { */
    /*         printf("%s(): s24c08_write() call returned an error - 0x%x\n", __func__, ret); */
    /*         return 1; */
    /*     } */

    /*     printf("%s(): Wrote 0x%02x to eeprom location 0x%x\n", __func__, value, default_address); */
    /* } */

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

    eeprom_args.end = arg_end(6);

    /* restore_eeprom_command_option_defaults(); */

    const esp_console_cmd_t eeprom_cmd = {
        .command = "eeprom",
        .help = "Write data to, or read data from the s24c08 eeprom",
        .hint = NULL,
        .func = &access_eeprom,
        .argtable = &eeprom_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&eeprom_cmd) );
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

