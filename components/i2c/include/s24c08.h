/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * s24c08c.h - Definitions for the S-2408CI 1Kbyte EEPROM
 */

#pragma once

/*
 * EEPROM memory is organized into four 256-byte blocks,
 * each referenced via a different I2C address.
 *
 * The four hi-order bits in the 7-bit I2C address are
 * fixed: 1010b. The fifth address byte is determined by
 * the connection of pin A2 to the board; in Omar's case,
 * an internal pulldown sets this bit to 0.
 *
 * So the first five bits of the address are 10100b.
 *
 * The final pair of bits determines the memory block
 * being accessed:
 *
 * Block0:  1010000b (0x50)
 * Block1:  1010001b (0x51)
 * Block2:  1010010b (0x52)
 * Block3:  1010011b (0x53)
 * 
 */

#define S24C08C_I2C_ADDRESS_BLOCK0      (0x50)
#define S24C08C_I2C_ADDRESS_BLOCK1      (0x51)
#define S24C08C_I2C_ADDRESS_BLOCK2      (0x52)
#define S24C08C_I2C_ADDRESS_BLOCK3      (0x53)

/*
 * S24C08C_I2C_ADDRESS_BADADDR is used to signal
 * an out-of-range address when attempting to map
 * EEPROM location address to i2c device address.
 */
#define S24C08C_I2C_ADDRESS_BADADDR     (0xff)

#define OMAR_EEPROM_SIZE                (0x400)

#define OMAR_EEPROM_PAGE_SIZE           (0x100)

#define OMAR_EEPROM_BLOCK0_MAXADDR      (0x0ff)
#define OMAR_EEPROM_BLOCK1_MAXADDR      (0x1ff)
#define OMAR_EEPROM_BLOCK2_MAXADDR      (0x2ff)
#define OMAR_EEPROM_BLOCK3_MAXADDR      (0x3ff)

// Functions:
void s24c08_init(void);
esp_err_t s24c08_random_read(uint16_t address, uint8_t *data);
esp_err_t s24c08_read(uint16_t address, uint8_t *data);