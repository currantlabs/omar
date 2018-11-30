/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * s24c08c.c - routines to control the S-2408CI 1Kbyte EEPROM
 */


#include <stdint.h>
#include <stdbool.h>
#include "hw_setup.h"
#include "driver/i2c.h"
#include "i2c.h"
#include "s24c08.h"
#include "esp_err.h"

#define ACK_CHECK_DIS                      0x0              /*!< I2C master will not check ack from slave */

static bool m_initialized = true;
static s24c08_eeprom_page_t map_eeprom_addr_to_device_addr(uint16_t addr);
static void s24c08_reset(void);
static void bit_bang_i2c_start(void);
static void bit_bang_i2c_stop(void);
static void bit_bang_i2c_clock(uint8_t cycles);
static esp_err_t s24c08_read_nbytes(s24c08_eeprom_page_t page, uint8_t *data, uint16_t count);


#define I2C_BIT_BANG_DELAY          (1/portTICK_PERIOD_MS)

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
    printf("%s(): initialized\n", __func__);
}

/*
 * s24c08_write() writes 1 byte to the specified
 * address (0x000 - 0x3ff) in EEPROM memory.
 *
 */
esp_err_t s24c08_write(uint16_t address, uint8_t data)
{
    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    s24c08_eeprom_page_t page = map_eeprom_addr_to_device_addr(address);
    if (page == S24C08C_I2C_PAGE_INVALID) {
        printf("%s(): address is out of range - 0x%x", __func__, address);
        return ESP_FAIL;
    }
    
    // Calculate the offset within the page to the desired location:
    uint8_t in_page_addr = (uint8_t )(address % OMAR_EEPROM_PAGE_SIZE);

    /*
     * Write the data to the s24c08 eeprom according to the procedure
     * defined in section "6.1 Byte Write" of the datasheet: send a
     * byte indicating the offset within the page, followed by the
     * byte you wish to write to eeprom memory.
     */
    uint8_t write_pkt[] = {in_page_addr, data};
    esp_err_t ret = i2c_tx(page, write_pkt, 2);
    if (ret != ESP_OK) {
        printf("%s(): failed to setup the address to read from\n", __func__);
        return ESP_FAIL;
    }

    return ESP_OK;

}

/*
 * s24c08_read() reads 1 byte from the specified
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

    s24c08_eeprom_page_t page = map_eeprom_addr_to_device_addr(address);
    if (page == S24C08C_I2C_PAGE_INVALID) {
        printf("%s(): address is out of range - 0x%x", __func__, address);
        return ESP_FAIL;
    }
    
    // Calculate the offset within the page to the desired location:
    uint8_t in_page_addr = (uint8_t )(address % OMAR_EEPROM_PAGE_SIZE);

    if (count + in_page_addr > OMAR_EEPROM_PAGE_SIZE) {
        printf("%s(): cannot read across page boundaries just yet (offset in page = 0x%02x, count = 0x%02x\n",
               __func__, in_page_addr, count);
        return ESP_FAIL;
    }

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

    return s24c08_read_nbytes(page, data, count);
    
    

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
 */
static s24c08_eeprom_page_t map_eeprom_addr_to_device_addr(uint16_t addr)
{
    if (addr <= OMAR_EEPROM_BLOCK0_MAXADDR) {
        return S24C08C_I2C_PAGE0;
    } else if (addr <= OMAR_EEPROM_BLOCK1_MAXADDR) {
        return S24C08C_I2C_PAGE1;
    } else if (addr <= OMAR_EEPROM_BLOCK2_MAXADDR) {
        return S24C08C_I2C_PAGE2;
    } else if (addr <= OMAR_EEPROM_BLOCK2_MAXADDR) {
        return S24C08C_I2C_PAGE3;
    } else {
        return S24C08C_I2C_PAGE_INVALID;
    }
}

