#include <driver/gpio.h>
#include "hw_setup.h"
#include "omar_als_timer.h"
#include "adi_spi.h"
#include "i2c.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iot_button.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/ledc.h"


static void gpio_setup(void);
#if defined(HW_OMAR)
static HwVersionT m_hw_version = HW_VERSION_UNKNOWN;
static int m_hw_version_raw_adc = 0;
static void led_setup(void);
static void button_setup(void);
static void plug_detect_setup(void);
static void adc_setup(void);
#endif  // HW_OMAR

void omar_setup(void)
{
    gpio_setup();
    i2c_init();
#if defined(HW_OMAR)
    button_setup();
    adc_setup();
    led_setup();
    timer_setup();
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

    /* 
     * Temporarily configure the i2c bus's
     * scl and sda lines as simple gpios so
     * that we can bit-bang our way through
     * the s24c08 eeprom chip's bizarre
     * setup sequence:
     */
    
    configure_gpio_output(I2C_SDA);
    configure_gpio_output(I2C_SCL);

}

#elif defined(HW_OMAR)

#ifdef CONFIGURING_MORE_GPIO_INPUTS
static void configure_gpio_input(uint8_t gpio)
{
    gpio_config_t gpio_cfg = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
    };
    gpio_cfg.pin_bit_mask = ((uint64_t)1 << gpio);
    gpio_config(&gpio_cfg);
}
#endif //CONFIGURING_MORE_GPIO_INPUTS


static void gpio_setup(void)
{
    /* Configure outputs */

    configure_gpio_output(OMAR_COIL_1_SET_GPIO);
    configure_gpio_output(OMAR_COIL_1_RESET_GPIO);
    configure_gpio_output(OMAR_COIL_2_SET_GPIO);
    configure_gpio_output(OMAR_COIL_2_RESET_GPIO);


    configure_gpio_output(OMAR_WHITE_LED0);
    configure_gpio_output(OMAR_WHITE_LED1);

    configure_gpio_output(ADI_RESET);
    gpio_set_level(ADI_RESET, true);    //Don't assert the ADI_RESET just yet:

    /* 
     * Temporarily configure the i2c bus's
     * scl and sda lines as simple gpios so
     * that we can bit-bang our way through
     * the s24c08 eeprom chip's bizarre
     * setup sequence:
     */
    
    configure_gpio_output(I2C_SDA);
    configure_gpio_output(I2C_SCL);

}

/*
 * Both leds share a single pwm timer:
 */
static ledc_timer_config_t ledc_timer = {
    .duty_resolution = OMAR_LED_DUTY_RESOLUTION,// resolution of PWM duty
    .freq_hz = OMAR_LEDC_FREQ_HZ,               // frequency of PWM signal
    .speed_mode = OMAR_LEDC_SPEED_MODE,         // timer mode
    .timer_num = OMAR_LEDC_TIMER                // timer index
};
    
/*
 * Index into the array of channel configs:
 */
#define OMAR_LED0_LEDCINDEX             (0)
#define OMAR_LED1_LEDCINDEX             (1)


static ledc_channel_config_t ledc_channel[] = {
    {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = OMAR_WHITE_LED0,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LEDC_CHANNEL_1,
        .duty       = 0,
        .gpio_num   = OMAR_WHITE_LED1,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    }
};


static void led_setup(void)
{
    ledc_timer_config(&ledc_timer);

    ledc_channel_config(&ledc_channel[OMAR_LED0_LEDCINDEX]);
    ledc_channel_config(&ledc_channel[OMAR_LED1_LEDCINDEX]);

}

static void update_relay_coil(gpio_num_t coil)
{

    // Energize the coil for 10 milliseconds - that's all it takes:
    gpio_set_level(coil, true);
    vTaskDelay(10/portTICK_PERIOD_MS);
    gpio_set_level(coil, false);

}

static void button_toggle_state(void)
{
    int on = toggle_white_led0(0, NULL);

    update_relay_coil(on ? OMAR_COIL_2_SET_GPIO : OMAR_COIL_2_RESET_GPIO);

}

static void button_toggle_state1(void)
{
    int on = toggle_white_led1(0, NULL);

    update_relay_coil(on ? OMAR_COIL_1_SET_GPIO : OMAR_COIL_1_RESET_GPIO);

}

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

    plug_detect_setup();

}

/*
 * led_gt_brightness() returns the duty cycle of the pwm channel
 * associated with the specified led.
 *
 * The led is specified by the assocaited gpio (so either
 * OMAR_WHITE_LED0 or OMAR_WHITE_LED1)
 * 
 */
uint32_t led_get_brightness(uint8_t led)
{
    uint8_t ch = (led == OMAR_WHITE_LED0 ? OMAR_LED0_LEDCINDEX : OMAR_LED1_LEDCINDEX);

    return ledc_get_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);

}

/*
 * NOTE: led_set_brightness() is not thread safe! See comments in "ledc.h" 
 * in the esp-idf sdk for details... If we decide to implement some kind
 * of "ui thread" and restrict all led manipulation to that thread, we're
 * ok with this implementation. Otherwise we'll have to revisit this..
 */
void led_set_brightness(uint8_t led, uint32_t duty)
{
    uint8_t ch = (led == OMAR_WHITE_LED0 ? OMAR_LED0_LEDCINDEX : OMAR_LED1_LEDCINDEX);

    if (duty > OMAR_LED_MAX_DUTY) {
        duty = OMAR_LED_MAX_DUTY;
    }

    ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, duty);
    ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);


#if defined(HW_SETUP_VERBOSE)
    printf("%s(): Set the duty cycle of led #%d to %d (hpoint is now %d)\n", 
           __func__,
           ch,
           duty,
           ledc_get_hpoint(ledc_channel[ch].speed_mode, ledc_channel[ch].channel));
#endif

}

/*
 * led_turnonoff() is used to toggle an led between
 * "full on" (duty cycle set to OMAR_LED_MAX_DUTY), and
 * "full off" (duty cycle is 0). 
 * 
 * It mimics the way we used to control the leds
 * before the PWM integration, when the leds were
 * just gpio lines that we pulled high or low.
 *
 */
static void led_turnonoff(uint8_t led, bool on)
{
    if (on) {
        led_set_brightness(led, OMAR_LED_MAX_DUTY);
    } else {
        led_set_brightness(led, 0);
    }
}

static void handle_plug_unplug_event(uint32_t plug, bool on)
{
    switch(plug) {

    case PLUG_DETECT1:
        led_turnonoff(OMAR_WHITE_LED0, on);
        break;

    case PLUG_DETECT2:
        led_turnonoff(OMAR_WHITE_LED1, on);
        break;

    default:
        printf("%s(): Invalid plug identifier %d\n", __func__, plug);
        break;
    }
}

static void plug_detect1_plug_cb(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        handle_plug_unplug_event(PLUG_DETECT1, true);
    }
}

static void plug_detect1_unplug_cb(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        handle_plug_unplug_event(PLUG_DETECT1, false);
    }
}

static void plug_detect2_plug_cb(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        handle_plug_unplug_event(PLUG_DETECT2, true);
    }
}

static void plug_detect2_unplug_cb(void* arg)
{
    static uint64_t previous;

    uint64_t current = xTaskGetTickCount();
    if ((current - previous) > DEBOUNCE_TIME) {
        previous = current;
        handle_plug_unplug_event(PLUG_DETECT2, false);
    }
}

static void plug_detect_setup(void)
{
    button_handle_t plug_detect1 = 
        iot_button_create_omar(
            PLUG_DETECT1,
            BUTTON_ACTIVE_LEVEL);

    if (plug_detect1) {
        iot_button_set_evt_cb(plug_detect1, BUTTON_CB_PUSH, plug_detect1_plug_cb, "PLUG");
        iot_button_set_evt_cb(plug_detect1, BUTTON_CB_RELEASE, plug_detect1_unplug_cb, "UNPLUG");
    }

    button_handle_t plug_detect2 = 
        iot_button_create_omar(
            PLUG_DETECT2,
            BUTTON_ACTIVE_LEVEL);

    if (plug_detect2) {
        iot_button_set_evt_cb(plug_detect2, BUTTON_CB_PUSH, plug_detect2_plug_cb, "PLUG");
        iot_button_set_evt_cb(plug_detect2, BUTTON_CB_RELEASE, plug_detect2_unplug_cb, "UNPLUG");
    }

}

int toggle_white_led0(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    led_turnonoff(OMAR_WHITE_LED0, on);

    return (on ? 1 : 0);
}

int toggle_white_led1(int argc, char** argv)
{
    static bool on = false;

    on = !on;

    led_turnonoff(OMAR_WHITE_LED1, on);

    return (on ? 1 : 0);
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

