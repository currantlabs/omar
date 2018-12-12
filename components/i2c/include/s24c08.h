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

typedef enum {
    S24C08C_I2C_PAGE0 = 0x50,
    S24C08C_I2C_PAGE1 = 0x51,
    S24C08C_I2C_PAGE2 = 0x52,
    S24C08C_I2C_PAGE3 = 0x53,
    S24C08C_I2C_PAGE_INVALID = 0xff,
} s24c08_eeprom_page_t;

#define OMAR_EEPROM_SIZE                (0x400)

#define OMAR_EEPROM_PAGE_SIZE           (0x100)

#define OMAR_EEPROM_BLOCK0_MAXADDR      (0x0ff)
#define OMAR_EEPROM_BLOCK1_MAXADDR      (0x1ff)
#define OMAR_EEPROM_BLOCK2_MAXADDR      (0x2ff)
#define OMAR_EEPROM_BLOCK3_MAXADDR      (0x3ff)

#define MAX_PAGE_WRITE                  (16)

#define OMAR_EEPROM_MAXADDR             (0x3ff)

/*
 * Re-setting the s24c08 requires some special manipulations
 * of the i2c bus that aren't supported by the esp-idf SDK
 * drivers -- so we "bit-bang" the reset sequence in. 
 * The constant I2C_BIT_BANG_DELAY defines the width of
 * the intervals during which the I2C_SCL line is held
 * high and low; the value of "1/portTICK_PERIOD_MS" was
 * arrived at empirically, and traces with the Saleae
 * logic analyzer confirm that a little less than 9 of
 * these intervals corresponds to 100 usec. Something
 * like this:
 *
 * 8.7 x (1/portTICK_PERIOD_MS) = 100 usec
 * ==> (1/portTICK_PERIOD_MS) = 11.4 usec (roughly)
 *
 * The I2C_BIT_BANG_DELAY constant is utilized in the
 * routines bit_bang_i2c_start(), bit_bang_i2c_clock()
 * and bit_bang_i2c_stop() that get called from the
 * reset procedure s24c08_reset() (all of which are 
 * static routines defined in s24c08.c).
 */
#define I2C_BIT_BANG_DELAY          (1/portTICK_PERIOD_MS)

/*
 * We clock omar's i2c bus at 400kHz, which is the maximum
 * bus speed the s24c08 supports. So one SCL period is 2.5 usec.
 *
 * The s24c08 supports both single "byte write" operations and
 * multi-byte "page write" operations. Given our SCL period of
 * 2.5 usec, we can calculate that it takes 70 usec to write a
 * single byte. 
 *
 * So the "page write" operation is the way to go for maximum
 * efficiency: up to 16 bytes can be written in a single burst,
 * and the the "raw" per-byte write throughput is 25.5 usec/byte.
 *
 * But we can't quite achieve this theoretical raw throughput
 * speed because the s24c08 needs to pause between page writes
 * to store the data in eeprom memory. We can minimize the length
 * of this pause by using a s24c08 feature called "acknowledge 
 * polling" to determine exactly when this internal write cycle
 * completes. But acknowledge polling requires a non-conventional
 * manipulation of the i2c signals that the esp-idf i2c drivers
 * don't support; plus, implementing polling increases code
 * complexity.
 *
 * So - what if we just wait? It turns out that waiting with a
 * "vTaskDelay(1/portTICK_PERIOD_MS)" is too short (the i2c
 * driver returns an error, presumably because the s24c08 doesn't
 * ACK when expected). But waiting "vTaskDelay(10/portTICK_PERIOD_MS)"
 * seems rock solid, and adding this 114 useconds to each 16-byte
 * page write brings the per-byte throughput down from the "raw"
 * speed of 25.5 usec/byte, to 32.6 usec/byte. 
 *
 * This means, if we were to write the entire 1KByte s24c08 eeprom
 * memory, it would take about 33.4 milliseconds. Given the way
 * we plan to use this memory, that seems acceptable. 
 * 
 * Constant S24C08_WRITE_DELAY defines how long we wait between
 * page write operations (10/portTICK_PERIOD_MS translates to
 * about 114 usec):
 */

#define S24C08_WRITE_DELAY          (10/portTICK_PERIOD_MS)

// Functions:
void s24c08_init(void);
esp_err_t s24c08_read(uint16_t address, uint8_t *data, uint16_t count);     // read some bytes
esp_err_t s24c08_write(uint16_t address, uint8_t *data, uint16_t count);    // write a byte


