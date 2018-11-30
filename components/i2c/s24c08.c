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
static uint8_t map_eeprom_addr_to_device_addr(uint16_t addr);
static esp_err_t s24c08_reset(void);

static esp_err_t s24c08_reset(void)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, 0xff, ACK_CHECK_DIS);
    i2c_master_start(cmd);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(OMAR_I2C_MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
	
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
 * s24c08_random_read() reads 1 byte from the specified
 * address (0x000 - 0x3ff) in EEPROM memory.
 *
 */
esp_err_t s24c08_random_read(uint16_t address, uint8_t *data)
{
    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    uint8_t i2c_devaddr = map_eeprom_addr_to_device_addr(address);
    if (i2c_devaddr == S24C08C_I2C_ADDRESS_BADADDR) {
        printf("%s(): address is out of range - 0x%x", __func__, address);
        return ESP_FAIL;
    }
    
    // Calculate the offset within the page to the desired location:
    uint8_t in_page_addr = (uint8_t )(address % OMAR_EEPROM_PAGE_SIZE);

    /*
     * First, set up the address to be read from (using the "dummy write" 
     * technique describd in section "7.2 Random read"of the s24c08
     * datasheet):
     */
    esp_err_t ret = i2c_tx(i2c_devaddr, &in_page_addr, 1);
    if (ret != ESP_OK) {
        printf("%s(): failed to setup the address to read from\n", __func__);
        return ESP_FAIL;
    }

    // Then just read the byte from that location:
    return s24c08_read(address, data);
    
}

/*
 * s24c08_read() reads 1 byte from the "current address"
 * maintained internally by the s24c08 EEPROM chip.
 * 
 * This address may have just been setup by a call to
 * s24c08_random_read(), or we may just be reading 
 * from successive locations in EEPROM and so relying
 * on the s24c08 to update its internal address for
 * us.
 *
 * 
 */
esp_err_t s24c08_read(uint16_t address, uint8_t *data)
{
    if (!m_initialized) {
        printf("%s(): the s24c08 hasn't been initialized\n", __func__);
        return ESP_FAIL;
    }

    uint8_t i2c_devaddr = map_eeprom_addr_to_device_addr(address);
    if (i2c_devaddr == S24C08C_I2C_ADDRESS_BADADDR) {
        printf("%s(): address is out of range - 0x%x", __func__, address);
        return ESP_FAIL;
    }
    
    // The s24c08 EEPROM chip knows the offset location
    // within the page specified by 'i2c_devaddr' from
    // which to read the data:
    esp_err_t ret = i2c_rx(i2c_devaddr, data, 1);
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
static uint8_t map_eeprom_addr_to_device_addr(uint16_t addr)
{
    if (addr <= OMAR_EEPROM_BLOCK0_MAXADDR) {
        return S24C08C_I2C_ADDRESS_BLOCK0;
    } else if (addr <= OMAR_EEPROM_BLOCK1_MAXADDR) {
        return S24C08C_I2C_ADDRESS_BLOCK1;
    } else if (addr <= OMAR_EEPROM_BLOCK2_MAXADDR) {
        return S24C08C_I2C_ADDRESS_BLOCK2;
    } else if (addr <= OMAR_EEPROM_BLOCK2_MAXADDR) {
        return S24C08C_I2C_ADDRESS_BLOCK3;
    } else {
        return S24C08C_I2C_ADDRESS_BADADDR;
    }
}

