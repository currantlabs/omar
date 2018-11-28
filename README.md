# Omar #

----------

A home for the Omar hardware bring-up code. Initially this will consist of a menu of console commands for testing the various hardware components (based on the esp-idf SDK's `console`).

This version of Omar is built around the Espressif ESP32, and you can find everything you need to know about the ESP32 platform and the esp-idf SDK used to build code for it here: [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/).

The official esp-idf SDK github repo can be found here: [Espressif IoT Development Framework](https://github.com/espressif/esp-idf). 

## Requirements ##

Follow the instructions at [What You Need](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#what-you-need) to install the toolchain and the ESP-IDF, and set up your build environment.

## Compile ##

The first time you check out the sources, you'll need to run `make menuconfig` to establish your configuration. The default configuration is fine, so just exit the `Espressif IoT Development Framework Configuration` menu UI by using the Right Arrow key to select and then press the `< Exit >` button at the bottom of the screen (and save if prompted).

Once you're past this initial configuration step, building is just the usual matter of invoking `make` and, once everything builds, saying `make flash`. 

## Running Code ##

`make flash` needs to know what TTY you're using, so be sure to setup the `ESPPORT` shell variable (i.e., `export ESPPORT=/dev/tty.usbserial`). And the default download speed is pretty slow, so you'll also want to say `export ESPBAUD=921600` to speed things up. 

If you're using a dev board like the [ESP32-PICO-KIT](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/get-started-pico-kit.html) and are using the micro-USB serial cable, you're all ready to run `make flash`. If on the other hand you're using a standard USB/Serial cable connected directly to the Rx/Tx pins (as you will if you're talking to Omar hardware), you'll need to manually put the board into download mode in order to flash. The ESP32 has a couple of pins called `EN` and `BOOT` that manage this for you, the idea is that you set `BOOT` and then reset the board by toggling `EN`: this brings the board up into download mode. Once the download is complete, clear `BOOT` and toggle `EN` to reset the board again, this time into normal boot mode where it jumps to your application.


