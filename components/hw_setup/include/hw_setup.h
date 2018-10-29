/* Console example — declarations of command registration functions.

   A simple framework for setting up gpios and the like for omar

 */

#pragma once

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

#define BLUE_LED    25
#define GREEN_LED   26
#define RED_LED     27 


#endif // HW_ESP32_DEVKITC

#ifdef    HW_ESP32_PICOKIT

#define BLUE_LED            23
#define GREEN_LED           18
#define RED_LED             5

#define BUTTON_GPIO          0
#define BUTTON_ACTIVE_LEVEL  0
#define DEBOUNCE_TIME       30


#endif // HW_ESP32_PICOKIT

// Configure the hardware
void omar_setup(void);

// Some console command:
int toggle_blue(int argc, char** argv);
int toggle_green(int argc, char** argv);
int toggle_red(int argc, char** argv);
