/* Console example — declarations of command registration functions.

   A simple framework for setting up gpios and the like for omar

 */

#pragma once

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

//#define HW_OMAR
//#define HW_ESP32_DEVKITC
#define HW_ESP32_PICOKIT

#ifdef    HW_ESP32_DEVKITC

#define BLUE_LED                        (25)
#define GREEN_LED                       (26)
#define RED_LED                         (27)


#endif // HW_ESP32_DEVKITC

#ifdef    HW_ESP32_PICOKIT

// Simulating the Omar i2c setup on the PicoKit devboard
#define I2C_SDA                         (0) 
#define I2C_SCL                         (4) 
#define OMAR_ESP32_I2C_CLOCKFREQHZ      (400000)
#define OMAR_I2C_MASTER_PORT            (I2C_NUM_1)

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
#define OMAR_WHITE_LED0                 (26)
#define OMAR_WHITE_LED1                 (27)

#define BUTTON_ACTIVE_LEVEL             (1)
#define DEBOUNCE_TIME                   (30)

#define OMAR_SWITCH_INT0                (36)   // aka "SENSOR_VP" (Pin 5)
#define OMAR_SWITCH_INT1                (39)   // aka "SENSOR_VN" (Pin 8)

// Omar plug detect:
#define PLUG_DETECT1                    (34)
#define PLUG_DETECT2                    (35)

// Omar Coils:
#define OMAR_COIL_1_SET_GPIO            (32)
#define OMAR_COIL_1_RESET_GPIO          (22)
#define OMAR_COIL_2_SET_GPIO            (33)
#define OMAR_COIL_2_RESET_GPIO          (21)

// Omar ADC inputs; both the ambient light sensor, and hw detect, are on ADC1:
#define VOUT_LGHT_SNSR                  (37)
#define VOUT_LGHT_SNSR__ADC_CHANNEL     (ADC_CHANNEL_1) 
#define HW_DET                          (38)
#define HW_DET__ADC_CHANNEL             (ADC_CHANNEL_2)


// Omar I2C:
#define I2C_SDA                         (0) 
#define I2C_SCL                         (4) 
#define OMAR_ESP32_I2C_CLOCKFREQHZ      (400000)
#define OMAR_I2C_MASTER_PORT            (I2C_NUM_1)


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
#endif  // HW_OMAR
