#include <driver/gpio.h>
#include "hw_setup.h"
#include "adi_spi.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"


static void gpio_setup(void);
static void button_setup(void);
#if defined(HW_OMAR)
static HwVersionT m_hw_version = HW_VERSION_UNKNOWN;
static int m_hw_version_raw_adc = 0;
static void adc_setup(void);
#endif	// HW_OMAR

void omar_setup(void)
{
    gpio_setup();
    button_setup();

#if defined(HW_OMAR)
    adc_setup();
#endif

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

static void adc_setup(void)
{
	

	// Configure the ambient light sensor ADC input:
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(VOUT_LGHT_SNSR__ADC_CHANNEL, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(HW_DET__ADC_CHANNEL, ADC_ATTEN_DB_0);

	

}

/*
 * hw_version_raw():
 * Reads the voltage from a resistor ladder - the ladder looks like this:
 *
 * GND - 10Kohm ->ADC<- 56K0hm - 3.3V
 *
 * If the ADC's full-scale 12-bit range is mapped onto 0 - 3.3V, 
 * then the reading should be (10/66)*(4096)=621, more or less.
 *
 * But if somehow the voltage range is mapped onto 0 - 5V, then
 * the result would be (50/33)*621=941
 *
 */
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

int hw_version_raw(void)
{
	int raw_adc;
	esp_adc_cal_characteristics_t *adc_chars;

    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = 
		esp_adc_cal_characterize(
			ADC_UNIT_1, 
			ADC_ATTEN_DB_0, 
			ADC_WIDTH_BIT_12, 
			DEFAULT_VREF, adc_chars); 

    print_char_val_type(val_type);

	raw_adc = adc1_get_raw(HW_DET__ADC_CHANNEL);

	//Convert adc_reading to voltage in mV
	uint32_t voltage = esp_adc_cal_raw_to_voltage(raw_adc, adc_chars);
	printf("Raw: %d\tVoltage: %dmV (Expected value is 500mV)\n", raw_adc, voltage);

	return raw_adc;
}

HwVersionT hw_version(void)
{
    if (m_hw_version != HW_VERSION_UNKNOWN) return m_hw_version;
    else {
        HwVersionT version = 
			(m_hw_version_raw_adc >= 110 && m_hw_version_raw_adc <= 200)
			?
			HW_VERSION_OMAR_1_0
			:
			HW_VERSION_UNKNOWN;
			
        m_hw_version = version;
        return version;
    }
}

int als_raw(void)
{

	return adc1_get_raw(VOUT_LGHT_SNSR__ADC_CHANNEL);

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

