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
    struct arg_str *operation;  // read or write
    struct arg_int *count;      // number of bytes to read 
    struct arg_int *address;    // Ranges from 0x000 to 0x400
    struct arg_int *value;      // (only for "write") Value to be written
    struct arg_str *values;     // a string of hex values to be written
    struct arg_end *end;
} eeprom_args;

static void restore_eeprom_command_option_defaults(void)
{
    eeprom_args.operation->sval[0] = "r";
    eeprom_args.count->ival[0] = 1;
    eeprom_args.address->ival[0] = 0;
    eeprom_args.value->ival[0] = 0xff;
    eeprom_args.values->sval[0] = "ff";
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
    static int default_address = 0;

    int nerrors = arg_parse(argc, argv, (void**) &eeprom_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, eeprom_args.end, argv[0]);
        restore_eeprom_command_option_defaults();
        return 1;
    }

    const char op = eeprom_args.operation->sval[0][0];

    if (!(op == 'w' || op == 'r')) {
        printf("eeprom: invalid operation \'%c\' (must be 'r' or 'w')\n", op);
        restore_eeprom_command_option_defaults();
        return 1;
    }

    const int value_specified = eeprom_args.value->count;
    if (value_specified && op == 'r') {
        printf("eeprom: don't specify a value when performing a read operation\n");
        restore_eeprom_command_option_defaults();
        return 1;
    }

    const int address_specified = eeprom_args.address->count;
    
    if (!address_specified) {
        default_address++;
    } else {
        // Check to see if the address is out of bounds:
        int address = eeprom_args.address->ival[0];

        if (address < 0 || address >= OMAR_EEPROM_SIZE) {
            printf("eeprom: address 0x%03x out of range (must be between 0x000 and 0x3ff)\n", address);
            restore_eeprom_command_option_defaults();
            return 1;
        }

        // Remember this address for next time
        default_address = eeprom_args.address->ival[0];
    }


    printf("%s(): Operation = [%s], count = %d, address = 0x%03x, and value = 0x%02x\n",
           __func__,
           eeprom_args.operation->sval[0],
           eeprom_args.count->ival[0],
           default_address,
           eeprom_args.value->ival[0]);


    uint8_t value = eeprom_args.value->ival[0];

    int count = eeprom_args.count->ival[0];

    bool multiple_write_values_specified = eeprom_args.values->count != 0;
    const char *write_values = NULL;

    if (multiple_write_values_specified) {
        write_values = eeprom_args.values->sval[0];
    }

    restore_eeprom_command_option_defaults();

    if (op == 'r' && multiple_write_values_specified) {
        printf("%s(): you cannot specify multiply write values when performing a read operation\n", __func__);
        return 1;
    }

    if (op == 'r' 
        && 
        (count + (default_address % OMAR_EEPROM_PAGE_SIZE)) > OMAR_EEPROM_PAGE_SIZE) {

        printf("%s(): can't read past the edge of a page of eeprom memory (base address = 0x%03x, count = 0x%02x)\n",
               __func__, default_address, count);

        return 1;

    }

    if (op == 'r') {
        uint8_t data[16] = {0};
        uint8_t *buf = (count <= 16 ? data : calloc(count, 1));
        int retval = 0; 

        esp_err_t ret = s24c08_read((uint16_t )default_address, buf, count);
        if (ret != ESP_OK) {
            printf("%s(): s24c08_read() call returned an error - 0x%x\n", __func__, ret);
            retval = 1;
        }

        if (count == 1) {
            printf("%s(): read 0x%02x from eeprom location 0x%x\n", __func__, buf[0], default_address);
        } else {
            for (int i=0; i<count/16; i++) {
                printf("0x%04x: ", default_address + i*16);
                for (int j=0; i*16+j < count && j < 16; j++) {
                    printf("0x%02x ", buf[i*16+j]);
                }
                printf("\n");
            }

            
        }

        // Clean up if you have to
        if (buf != data) {
            free(buf);
        }

        return retval;

    }

    // It's a write operation:

    if (op == 'w' && !address_specified) {
        printf("%s(): must specify the address when writing data\n", __func__);
        return 0;
    }

    if (multiple_write_values_specified) {

        if (!valid_hexadecimal_value(write_values)) {
            return 1;
        }

        printf("%s(): attempting to write multiple values to a location: [%s]\n", __func__, write_values);
        return 0;
    }

    esp_err_t ret = s24c08_write(default_address, value);
    if (ret != ESP_OK) {
        printf("%s(): s24c08_read() call returned an error - 0x%x\n", __func__, ret);
        return 1;
    }

    printf("%s(): Wrote 0x%02x to eeprom location 0x%x\n", __func__, value, default_address);

    return 0;
}

static void register_eeprom()
{
    eeprom_args.operation = arg_str0(NULL, NULL, "<r|w>", "Operation to perform, (r)ead or (w)rite (defaults to \"r\")");
    eeprom_args.count = arg_int0(NULL, "count", "<c>", "Number of bytes to read (cannot read past the end of a 256-byte page)");
    eeprom_args.address = arg_int0(NULL, "address", "<a>", "Address to access, 0x000 - 0x3ff (for reads, defaults to 0x000, or to the last specified address)");
    eeprom_args.value = arg_int0(NULL, "value", "<v>", "Value to be written (defaults to 0xff)");
    eeprom_args.values = arg_str0(NULL, "values", NULL, "A string of hexadecimal digits");
    eeprom_args.end = arg_end(4);

    restore_eeprom_command_option_defaults();

    const esp_console_cmd_t eeprom_cmd = {
        .command = "eeprom",
        .help = "Access the s24c08 eeprom",
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

