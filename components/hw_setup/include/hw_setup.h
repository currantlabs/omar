/* Console example — declarations of command registration functions.

   A simple framework for setting up gpios and the like for omar

 */

#pragma once

/*
 * Macro to check the outputs of TWDT functions and trigger an abort if an
 * incorrect code is returned.
 */
#define CHECK_ERROR_CODE(returned, expected) ({                        \
            if(returned != expected){                                  \
                printf("TWDT ERROR\n");                                \
                abort();                                               \
            }                                                          \
})


typedef enum {
    HW_VERSION_UNKNOWN,
    HW_VERSION_DEVKIT,
    HW_VERSION_DEBUG,
    HW_VERSION_1_0,
    HW_VERSION_2_0,
    HW_VERSION_3_0,
    HW_VERSION_WALLACE_LATEST,
    HW_VERSION_STRINGER_1_0,
    HW_VERSION_STRINGER_1_1,
    HW_VERSION_OMAR_1_0,
    //NOTE: be sure to include new  *_STRINGER_* in hw_version_is_stringer()
    //and *_OMAR_* in hw_version_is_omar()
} HwVersionT;



/*
 * Using a couple of different esp32 development boards
 * while waiting for actual omar hardware to arrive.
 *
 * Depending on which dev board is being used, some gpio
 * assignemnts etc. will change.
 *
 * HW_ESP32_DEVKITC
 * Refers to the "ESP32-DevKitC development board", the
 * devboard we were given to use during GridConnect 
 * esp32 training up in SF on Oct 18-19 2018. Works 
 * fine but is built around the ESP-WROOM-32 module,
 * which is different from the module we'll be using
 * with actual omar hardware
 *
 * HW_ESP32_PICOKIT
 * Refers to the "ESP32-PICO-KIT development board"
 * which resembles actual omar hardware because they 
 * both use the same esp32 module, the ESP32-PICO-D4.
 * The set of gpios available to play with on the
 * HW_ESP32_PICOKIT more closely resembles those 
 * available on the actual omar hardware
 *
 */

#define HW_OMAR
//#define HW_ESP32_DEVKITC
//#define HW_ESP32_PICOKIT

#ifdef    HW_ESP32_DEVKITC

#define BLUE_LED                        (25)
#define GREEN_LED                       (26)
#define RED_LED                         (27)


#endif // HW_ESP32_DEVKITC

#ifdef    HW_ESP32_PICOKIT

#define BLUE_LED                        (21)
#define GREEN_LED                       (22)
#define RED_LED                         (9)

#define BUTTON_ACTIVE_LEVEL             (0)
#define DEBOUNCE_TIME                   (30)

// ADI7953 Energy Monitor chip signals
#define ADI_RESET                       (10)    // Just a GPIO output. (RESET_N_MON)
#define OMAR_SPIM0_SCK_PIN              (18)    // SPI clock GPIO pin number. (SPI_MON_CLK)
#define OMAR_SPIM0_MOSI_PIN             (23)    // SPI Master Out Slave In GPIO pin number. (SPI_MON_MOSI)
#define OMAR_SPIM0_MISO_PIN             (19)    // SPI Master In Slave Out GPIO pin number. (SPI_MON_MISO)
#define OMAR_SPIM0_SS_PIN               (5)     // SPI Slave Select GPIO pin number. (SPI_MON_CS)

// Omar LEDs and Buttons
#define OMAR_COIL_1_SET_GPIO            (32)
#define OMAR_COIL_1_RESET_GPIO          (22)
#define OMAR_COIL_2_SET_GPIO            (33)
#define OMAR_COIL_2_RESET_GPIO          (21)

#define OMAR_WHITE_LED0                 (26)
#define OMAR_WHITE_LED1                 (27)


// Don't implement AD7953 interrupt support just yet:
//#define ADE7953_INTERRUPT_SUPPORT

#endif // HW_ESP32_PICOKIT

#if defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)
// Note: sometimes it's useful to be able to emulate
// omar features on the ESP32-PICO-KIT devboard so that
// I can examine signals with the logic analyzer. So
// I set up this "common" #defines section..

// Omar I2C:
#define I2C_SDA                         (0) 
#define I2C_SCL                         (4) 
#define OMAR_ESP32_I2C_CLOCKFREQHZ      (400000)
#define OMAR_I2C_MASTER_PORT            (I2C_NUM_1)

// Omar Coils:
#define OMAR_COIL_1_SET_GPIO            (32)
#define OMAR_COIL_1_RESET_GPIO          (22)
#define OMAR_COIL_2_SET_GPIO            (33)
#define OMAR_COIL_2_RESET_GPIO          (21)


#endif //defined(HW_OMAR) || defined(HW_ESP32_PICOKIT)

#ifdef    HW_OMAR

// ADI7953 Energy Monitor chip signals
#define ADI_RESET                       (10)    // Just a GPIO output. (RESET_N_MON)
#define OMAR_SPIM0_SCK_PIN              (18)    // SPI clock GPIO pin number. (SPI_MON_CLK)
#define OMAR_SPIM0_MOSI_PIN             (23)    // SPI Master Out Slave In GPIO pin number. (SPI_MON_MOSI)
#define OMAR_SPIM0_MISO_PIN             (19)    // SPI Master In Slave Out GPIO pin number. (SPI_MON_MISO)
#define OMAR_SPIM0_SS_PIN               (5)     // SPI Slave Select GPIO pin number. (SPI_MON_CS)
// Don't implement AD7953 interrupt support just yet:
//#define ADE7953_INTERRUPT_SUPPORT


// Omar LEDs and Buttons

/*
 * With the LED PWM resolution set to LEDC_TIMER_13_BIT
 * we've can set the duty cycle to anything between 0
 * and 2^13-1 = 8191. 
 */
#define OMAR_LED_DUTY_RESOLUTION        (LEDC_TIMER_13_BIT)

// OMAR_LED_MAX_DUTY is 8192 (LEDC_TIMER_13_BIT, and 2^^13 = 8192-1)
#define OMAR_LED_MAX_DUTY               (8191)
#define OMAR_LEDC_TIMER                 (LEDC_TIMER_0)
#define OMAR_LEDC_SPEED_MODE            (LEDC_HIGH_SPEED_MODE)
#define OMAR_LEDC_FREQ_HZ               (5000)

#define OMAR_WHITE_LED0                 (26)
#define OMAR_WHITE_LED1                 (27)

#define BUTTON_ACTIVE_LEVEL             (1)
#define DEBOUNCE_TIME                   (30)

#define OMAR_SWITCH_INT0                (36)   // aka "SENSOR_VP" (Pin 5)
#define OMAR_SWITCH_INT1                (39)   // aka "SENSOR_VN" (Pin 8)

// Omar plug detect:
#define PLUG_DETECT1                    (34)
#define PLUG_DETECT2                    (35)

// Omar ADC inputs; both the ambient light sensor, and hw detect, are on ADC1:
#define VOUT_LGHT_SNSR                  (37)
#define VOUT_LGHT_SNSR__ADC_CHANNEL     (ADC_CHANNEL_1) 
#define HW_DET                          (38)
#define HW_DET__ADC_CHANNEL             (ADC_CHANNEL_2)

// At fixed intervals we "pause" the pwm led drive, turning
// the leds off so that we can sample the ambient light using
// the als output at VOUT_LGHT_SNSR. The constant ALS_SAMPLE_DELAY
// specifies how long after this pause to wait before reading
// the als; practically speaking there should be no need for
// any delay (speed of light, duh) -- but there may be "slew effects"
// from the pwm led controller regarding waiting for the current
// pwm "period" to expire before the led output changes state
// etc etc..
// This odd delay, (1/portTICK_PERIOD_MS), I've used elsewhere
// and used a logic analyzer to measure the resulting delay as
// 11.4 usec.
#define ALS_SAMPLE_DELAY                (1/portTICK_PERIOD_MS)


#endif // HW_OMAR



// Configure the hardware
void omar_setup(void);

// Some console command:
int toggle_white_led0(int argc, char** argv);
int toggle_white_led1(int argc, char** argv);

#if defined(HW_ESP32_PICOKIT)
int toggle_blue(int argc, char** argv);
int toggle_green(int argc, char** argv);
int toggle_red(int argc, char** argv);
#endif //HW_ESP32_PICOKIT

// Miscellaneous commands:

#if defined(HW_OMAR)
HwVersionT hw_version(void);
int hw_version_raw(void);
int als_raw(void);
uint32_t led_get_brightness(uint8_t led);
void led_set_brightness(uint8_t led, uint32_t duty);
#endif  // HW_OMAR
