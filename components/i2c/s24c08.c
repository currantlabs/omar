/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * s24c08c.c - routines to control the S-2408CI 1Kbyte EEPROM
 */


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hw_setup.h"
#include "driver/i2c.h"
#include "i2c.h"
#include "s24c08.h"
#include "esp_err.h"

static bool m_initialized = false;

static s24c08_eeprom_page_t map_eeprom_addr_to_device_addr(uint16_t addr);
static void s24c08_reset(void);
static void bit_bang_i2c_start(void);
static void bit_bang_i2c_stop(void);
static void bit_bang_i2c_clock(uint8_t cycles);
static uint16_t number_of_pages_spanned(uint16_t address, uint16_t count);
static esp_err_t s24c08_read_nbytes(s24c08_eeprom_page_t page, uint8_t *data, uint16_t count);
static esp_err_t s24c08_write_page(uint16_t address, uint8_t *data, uint16_t count);
static esp_err_t s24c08_write_up_to_16_bytes(s24c08_eeprom_page_t page, uint8_t *data, uint16_t count);

static void bit_bang_i2c_clock(uint8_t cycles)
{
    // Make sure I2C_SCL is low:
    gpio_set_level(I2C_SCL, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    for (uint8_t cycle=0; cycle < cycles; cycle++) {
        gpio_set_level(I2C_SCL, true);
        vTaskDelay(I2C_BIT_BANG_DELAY);
        gpio_set_level(I2C_SCL, false);
        vTaskDelay(I2C_BIT_BANG_DELAY);
    }

}

static void bit_bang_i2c_start(void)
{
    /* 
     * A high-to-low transition on I2C_SDA
     * while I2C_SCL is high is interpreted
     * as an i2c 'start' condition:
     */

    // Make sure I2C_SCL is low:
    gpio_set_level(I2C_SCL, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // Make sure I2C_SDA is high:
    gpio_set_level(I2C_SDA, true);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // I2C_SCL goes high:
    gpio_set_level(I2C_SCL, true);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // While I2C_SCL is high, bring
    // I2C_SDA down:
    gpio_set_level(I2C_SDA, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // Finally bring I2C_SCL down
    gpio_set_level(I2C_SCL, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

}

static void bit_bang_i2c_stop(void)
{
    /* 
     * A low-to-high transition on I2C_SDA
     * while I2C_SCL is high is interpreted
     * as an i2c 'start' condition:
     */

    // Make sure I2C_SCL is low:
    gpio_set_level(I2C_SCL, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // Make sure I2C_SDA is low:
    gpio_set_level(I2C_SDA, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // I2C_SCL goes high:
    gpio_set_level(I2C_SCL, true);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // While I2C_SCL is high, bring
    // I2C_SDA high:
    gpio_set_level(I2C_SDA, true);
    vTaskDelay(I2C_BIT_BANG_DELAY);

    // Finally bring I2C_SCL down
    gpio_set_level(I2C_SCL, false);
    vTaskDelay(I2C_BIT_BANG_DELAY);

}

static void s24c08_reset(void)
{

    bit_bang_i2c_start();

    gpio_set_level(I2C_SDA, true);

    bit_bang_i2c_clock(9);

    bit_bang_i2c_start();

    bit_bang_i2c_stop();

}


/*
 * See the reset procedure on pg 22 of the datasheet
 * (section "[How to reset S-24C08C]")
 */
void s24c08_init(void)
{
    s24c08_reset();
    m_initialized = true;
}

/*
 * s24c08_write_up_to_16_bytes() takes advantage of the s24c08 eeprom's
 * optimized "page write" mode, which allows you to write as many as
 * 16 bytes in a single i2c transaction.
 *
 * In "page write" mode the s24c08's internal write address register
 * only uses the least-significant 4 bits and so you must pay attention
 * to where you're writing wrt 16-byte address boundaries. If you try
 * to write 16 bytes from an address of the form Nx16 + 7, the first
 * 9 bytes will go were you expect but the final 7 bytes will wrap
 * around behind you, starting at address Nx16.
 *
 * s24c08_write_page() takes the alignment of the write data with
 * respect to multiple-of-16 byte boundaries, and is careful to call
 * s24c08_write_up_to_16_bytes() so as to avoid this wrap-around 
 * issue - breaking down the write data as much as possible into
 * 16-byte chunks aligned with multiple of 16 addresses.
 *
 */

static esp_err_t s24c08_write_up_to_16_bytes(s24c08_eeprom_page_t page, uint8_t *data, uint16_t count)
{
    esp_err_t status;

#if defined(S24C08_VERBOSE)
    printf("%s(0x%02x, {0x%02x, 0x%02x, 0x%02x, 0x%02x}, %d)\n", 
           __func__, 
           page, 
           data[0], data[1], data[2], data[3], 
           count);
#endif

    if (count == 0) {
        return ESP_OK;
    }

    if ((status=i2c_tx(page, data, count)) != ESP_OK) {
        printf("%s(): failed to write the %d bytes of data to address 0x%02x\n",
               __func__,
               count,
               data[0]);

        return status;
    }

    // Pause for a bit to give the s24c08 eeprom
    // time to complete the write operation..
    vTaskDelay(S24C08_WRITE_DELAY);
    

    return ESP_OK;
}

/*
 * s24c08_write_page() writes data to a single contiguous "page" of 
 * s24c08 eeprom memory. 
 *
 * These pages are OMAR_EEPROM_PAGE_SIZE (256) bytes long. 
 * Depending on the circumstances, you could be writing a single
 * byte somewhere on the page -- or you could be filling the 
 * entire page. 
 *
 * But the s24c08 eeprom is limited to burst writes of at most
 * 16 bytes at a time (16 bytes can be written if the start
 * address -- the offset within the page -- is a multiple of 16).
 *
 * s24c08_write_page() breaks the write operation up into chunks 
 * that dovetail with addresses at 16-byte boundaries, and calls
 * s24c08_write_up_to_16_bytes() to actually write them into the
 * eeprom.
 *
 */
static esp_err_t s24c08_write_page(uint16_t address, uint8_t *data, uint16_t count)
{
    esp_err_t status;
    uint16_t bytes_to_write = count;
    s24c08_eeprom_page_t page = map_eeprom_addr_to_device_addr(address);
    if (page == S24C08C_I2C_PAGE_INVALID) {
        printf("%s(): address is out of range - 0x%x", __func__, address);
        return ESP_FAIL;
    }

#if defined(S24C08_VERBOSE)
    printf("%s(): Writing %d bytes to address 0x%03x (page = 0x%02x)\n",
           __func__,
           count,
           address,
           page);
#endif

    // You'll never write more than 16 bytes of data at once,
    // so including the initial address byte, your "write_buffer"
    // needs to be 17 bytes big:
    uint8_t write_buffer[17] = {0};

    uint8_t in_page_address = (uint8_t )(address % OMAR_EEPROM_PAGE_SIZE);
    uint8_t current_address = in_page_address;

    // If you're not already lined up on a 16-byte boundary,
    // write just enough bytes to take you up to one:
    if (in_page_address % 16 != 0) {

        // Write enough bytes to bring you up to the next
        // 16-byte boundary address:
        uint8_t chunk_size = 16 - (in_page_address % 16);
        
        // Don't try to write too much by mistake:
        if (chunk_size > bytes_to_write) {
            chunk_size = bytes_to_write;
        }

        write_buffer[0] = current_address;
        memcpy(&write_buffer[1], data, chunk_size);

#if defined(S24C08_VERBOSE)
        printf("%s(): writing an odd number of bytes to roll up to next 16-byte boundary  - writing %d bytes to address 0x%03x\n",
               __func__,
               chunk_size,
               current_address);
#endif

        status = s24c08_write_up_to_16_bytes(page, write_buffer, chunk_size+1);
        if (status != ESP_OK) {
            printf("%s(): Failed writing to write a %d-byte chunk of data to address 0x%03x\n",
                   __func__,
                   chunk_size,
                   current_address);
            return status;
        }

        current_address += chunk_size;
        bytes_to_write -= chunk_size;

        
    }

    // Consistency check:
    if ((bytes_to_write > 0)
        &&
        (current_address % 16 != 0)) {
        printf("%s(): Should be at at a 16-byte boundary but we're not: current address is 0x%03x, with %d bytes to go..\n",
               __func__,
               current_address,
               bytes_to_write);

        return ESP_FAIL;
    }

    while (bytes_to_write > 0) {
        uint8_t chunk_size = (bytes_to_write > 16 ? 16 : bytes_to_write);
        uint8_t offset = current_address - in_page_address;

        write_buffer[0] = current_address;
        memcpy(&write_buffer[1], &data[offset], chunk_size);
        status = s24c08_write_up_to_16_bytes(page, write_buffer, chunk_size+1);
        if (status != ESP_OK) {
            printf("%s(): Failed writing to write a %d-byte chunk of data to address 0x%03x\n",
                   __func__,
                   chunk_size,
                   current_address);
            return status;
        }

        current_address += chunk_size;
        bytes_to_write -= chunk_size;


    }

    return ESP_OK;


}

/*
 * number_of_pages_spanned() figures out how many distinct pages
 * a given read or write operation will access. 
 *
 * "pages" here is used in the sense of the 4 distinct regions
 * in the s24c08 eeprom (each OMAR_EEPROM_PAGE_SIZE or 256 bytes
 * in size) that need go be addressed via distinct i2c device
 * numbers. 
 * 
 * Any read/write (even of a single byte) accesses at least
 * one such page. Depending on the number of bytes, and the
 * address at which the read/write access begins, the number
 * of distinct pages affected will grow.
 *
 * Suppose you want to read 250 bytes. If your access begins
 * at an address less than or equal to 5, all 250 bytes reside
 * on the same page ("page 0," the page corresponding to i2c
 * device address S24C08C_I2C_PAGE0 or 0x50). 
 *
 * But if instead you need to access 250 bytes starting at
 * address 80, then you'll cross the page boundary at 0x100
 * and the access will end up touching 2 pages.
 *
 * number_of_pages_spanned() simply consolidates this in one place..
 *
 */
static uint16_t number_of_pages_spanned(uint16_t address, uint16_t count)
{
    uint16_t pages =
        1 // Even a one-byte write is on a page somewhere.
        +
        (   // Add in extra page-spans for each time you cross a page boundary:
            ((address + count)/OMAR_EEPROM_PAGE_SIZE) 
            - 
            (address/OMAR_EEPROM_PAGE_SIZE)
        );


    /*
     * In cases where the access touches the last byte in eeprom,
     * the preceding formula can yield 5 - catch that case
     * and fix it (the max number of pages spanned is 4):
     */
    pages = (pages > 4) ? 4 : pages;


    return pages;
}

/*
 * s24c08_write() writes 'count' bytes to the specified
 * address (0x000 - 0x3ff) in EEPROM memory.
 *
 */
esp_err_t s24c08_write(uint16_t address, uint8_t *data, uint16_t count)
{
    if (count == 0) {
        return ESP_OK;
    }

    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    if (count + address > OMAR_EEPROM_SIZE) {
        printf("%s(): you are attempting to write past the end of eeprom (0x%04x) (start address = 0x%04x, count = 0x%04x)\n",
               __func__, OMAR_EEPROM_SIZE-1, address, count);
        return ESP_FAIL;
    }

    uint16_t pages_spanned = number_of_pages_spanned(address, count);

    uint16_t bytes_to_write_to_this_page = OMAR_EEPROM_PAGE_SIZE - (address % OMAR_EEPROM_PAGE_SIZE);
    bytes_to_write_to_this_page = (bytes_to_write_to_this_page > count ? count : bytes_to_write_to_this_page);

    uint16_t current_address = address;
    uint16_t final_address = address + count;
    uint8_t *current_data_ptr = data;

#if defined(S24C08_VERBOSE)
    printf("%s(): Writing %d bytes to address 0x%04x, spanning %d different pages\n",
           __func__, 
           count, 
           address, 
           pages_spanned);
#endif

    for (int i=0; i<pages_spanned; i++) {

#if defined(S24C08_VERBOSE)
        printf("%s(): Writing to eeprom page %d of %d (current_address = 0x%04x, final_address = 0x%04x)\n",
               __func__,
               i+1,
               pages_spanned,
               current_address,
               final_address);
#endif

        if (ESP_OK != 
            s24c08_write_page(
                current_address, 
                current_data_ptr, 
                bytes_to_write_to_this_page)) {
            printf("%s(): Failed writing to page %d of %d (current_address = 0x%04x)\n", 
                   __func__, 
                   i, 
                   pages_spanned,
                   current_address);

            return ESP_FAIL;

        }

        current_address += bytes_to_write_to_this_page;
        current_data_ptr += bytes_to_write_to_this_page;
        uint16_t bytes_to_go = final_address - current_address;
        bytes_to_write_to_this_page =
            (bytes_to_go > OMAR_EEPROM_PAGE_SIZE)
            ?
            OMAR_EEPROM_PAGE_SIZE
            :
            bytes_to_go;

    }

    return ESP_OK;
}

    
/*
 * s24c08_read() reads 'count' bytes from the specified
 * address (0x000 - 0x3ff) in EEPROM memory.
 *
 */
esp_err_t s24c08_read(uint16_t address, uint8_t *data, uint16_t count)
{
    if (count == 0) {
        return ESP_OK;
    }

    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    if (count + address > OMAR_EEPROM_SIZE) {
        printf("%s(): you are attempting to read past the end of eeprom (0x%04x) (start address = 0x%04x, count = 0x%04x)\n",
               __func__, OMAR_EEPROM_SIZE-1, address, count);
        return ESP_FAIL;
    }

    // Because s24c08_read_nbytes() can only read data from one
    // contiguous 256-byte "page" of s24c08 eeprom memory, figure
    // out how many eeprom pages are spanned, and thus how many
    // invocations are required:

    uint16_t pages_spanned = number_of_pages_spanned(address, count);
    uint16_t bytes_to_read_from_this_page = OMAR_EEPROM_PAGE_SIZE - (address % OMAR_EEPROM_PAGE_SIZE);
    uint16_t current_address = address;
    uint16_t final_address = address + count;

#if defined(S24C08_VERBOSE)
    printf("%s(): Reading %d bytes from address 0x%04x\n",__func__, count, address);
#endif

    for (int i=0; i<pages_spanned; i++) {

#if defined(S24C08_VERBOSE)
        printf("%s(): Reading from eeprom page %d of %d (current_address = 0x%04x, final_address = 0x%04x)\n", 
               __func__,
               i,
               pages_spanned,
               current_address,
               final_address);
#endif

        s24c08_eeprom_page_t page = map_eeprom_addr_to_device_addr(current_address);
        if (page == S24C08C_I2C_PAGE_INVALID) {
            printf("%s(): address is out of range - 0x%04x\n", __func__, current_address);
            return ESP_FAIL;
        }
    
        // Calculate the offset within the page to the desired location:
        uint8_t in_page_addr = (uint8_t )(current_address % OMAR_EEPROM_PAGE_SIZE);

        /*
         * First, set up the address to be read from (using the "dummy write" 
         * technique describd in section "7.2 Random read"of the s24c08
         * datasheet):
         */
        esp_err_t ret = i2c_tx(page, &in_page_addr, 1);
        if (ret != ESP_OK) {
            printf("%s(): failed to setup the address to read from\n", __func__);
            return ESP_FAIL;
        }

        if (ESP_OK != 
            s24c08_read_nbytes(
                page, 
                &data[current_address - address], 
                bytes_to_read_from_this_page)) {

            printf("%s(): Failed reading from page %d of %d (current_address = 0x%04x)\n", 
                   __func__, 
                   i, 
                   pages_spanned,
                   current_address);

            return ESP_FAIL;

        }
        
        current_address += bytes_to_read_from_this_page;

        uint16_t bytes_to_go = final_address - current_address;

        bytes_to_read_from_this_page = 
            (bytes_to_go > OMAR_EEPROM_PAGE_SIZE) 
            ?
            OMAR_EEPROM_PAGE_SIZE
            :
            bytes_to_go;

    }

    return ESP_OK;
}

/*
 * s24c08_read_nbytes() reads n bytes from the "current address"
 * maintained internally by the s24c08 EEPROM chip.
 * 
 * This address may have just been setup by a call to
 * s24c08_read(), or we may just be reading 
 * from successive locations in EEPROM and so relying
 * on the s24c08 to update its internal address for
 * us.
 *
 * 
 */
static esp_err_t s24c08_read_nbytes(s24c08_eeprom_page_t page, uint8_t *data, uint16_t count)
{
    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    if (page == S24C08C_I2C_PAGE_INVALID) {
        printf("%s(): bad page - 0x%x", __func__, page);
        return ESP_FAIL;
    }
    
    // The s24c08 EEPROM chip knows the offset location
    // within the page specified by 'page' from
    // which to read the data:
    esp_err_t ret = i2c_rx(page, data, count);
    if (ret != ESP_OK) {
        printf("%s(): failed to read the requested data from the s24c08 EEPROM\n", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;

}

/*
 * The s24c08 EEPROM part responds to 4 distinct
 * i2c device addresses, depending on the address
 * of the location in memory being accessed.
 *
 * map_eeprom_addr_to_device_addr() takes an address
 * in the range 0-0x3ff and maps it onto the 
 * corresponding s24c08 block's i2c device address.
 */
static s24c08_eeprom_page_t map_eeprom_addr_to_device_addr(uint16_t addr)
{
    if (addr <= OMAR_EEPROM_BLOCK0_MAXADDR) {
        return S24C08C_I2C_PAGE0;
    } else if (addr <= OMAR_EEPROM_BLOCK1_MAXADDR) {
        return S24C08C_I2C_PAGE1;
    } else if (addr <= OMAR_EEPROM_BLOCK2_MAXADDR) {
        return S24C08C_I2C_PAGE2;
    } else if (addr <= OMAR_EEPROM_BLOCK3_MAXADDR) {
        return S24C08C_I2C_PAGE3;
    } else {
        return S24C08C_I2C_PAGE_INVALID;
    }
}

