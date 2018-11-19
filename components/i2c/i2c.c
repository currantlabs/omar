/* Copyright (c) 2015 Currant Inc. All Rights Reserved.
 *
 * i2c.c - routines to communicate to slave peripherials via I2C, using the TWI module.
 */

#include "driver/i2c.h"
#include "i2c.h"
#include "hw_setup.h"
#include "s5852a.h"



#if     defined(NEW_DAY)
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nrf_drv_twi.h"
#include "app_error.h"
#include "nrf.h"
#include "bsp.h"
#include "console.h"
#include "i2c.h"
#include "log.h"
#include "app_util_platform.h"
#include "version.h"

#define MASTER_TWI_INST          0    //TWI interface used as a master (overlaps with SPIM0)

static const nrf_drv_twi_t m_twi_master = NRF_DRV_TWI_INSTANCE(MASTER_TWI_INST);

static ret_code_t twi_master_init(void);
static int console_command(int argc, char *argv[]);

#endif//defined(NEW_DAY)

void i2c_init(void)
{

    int i2c_master_port = OMAR_I2C_MASTER_PORT; 
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = OMAR_ESP32_I2C_CLOCKFREQHZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);

    s5852a_init();

}

#if     defined(NEW_DAY)
static ret_code_t twi_master_init(void)
{
    ret_code_t ret;

    uint8_t scl, sda;

    if (hw_version_is_omar()) {
        scl = OMAR_TWI_SCL_M;
        sda = OMAR_TWI_SDA_M;
    } else {
        scl = WALLACE_TWI_SCL_M;
        sda = WALLACE_TWI_SDA_M;
    }

    const nrf_drv_twi_config_t config = {
        .scl                = scl,
        .sda                = sda,
        .frequency          = NRF_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH
    };

    ret = nrf_drv_twi_init(&m_twi_master, &config, NULL, NULL);
    if(NRF_SUCCESS != ret) {
        return ret;
    }

    nrf_drv_twi_enable(&m_twi_master);

    return ret;
}
#endif//defined(NEW_DAY)


esp_err_t i2c_tx(uint8_t address, uint8_t* data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( address << 1 ) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(OMAR_I2C_MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

//FIXME: change API, we don't need xfer_pending for i2c_rx
esp_err_t i2c_rx(uint8_t address, uint8_t *data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( address << 1 ) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(OMAR_I2C_MASTER_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}


#if     defined(NEW_DAY)
static void usage(void)
{
    LOG(LOG_LEVEL_DEBUG, "usage: i2c write  -- simple write test\r\n");
}

#define DATA_LEN (8)
static int console_command(int argc, char *argv[])
{
    if ((argc == 1) && (strcmp(argv[0], "write") == 0)) {
        uint8_t data[DATA_LEN] = {0x01,0x02,0x03,0xff,0xae,0x55,0xa5,0x5a};
        ret_code_t ret = nrf_drv_twi_tx(&m_twi_master, 0x50, data, DATA_LEN, false);
        LOG(LOG_LEVEL_DEBUG, "wrote data, ret=%d\r\n", (int)ret);
        return ret;
    } else if ((argc == 1) && (strcmp(argv[0], "read") == 0)) {
        uint8_t data[DATA_LEN];
        ret_code_t ret = nrf_drv_twi_rx(&m_twi_master, 0x50, data, DATA_LEN);
        LOG(LOG_LEVEL_DEBUG, "read data: %02x %02x %02x %02x %02x %02x %02x %02x \r\n", data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
        return ret;
    }

    else usage();
    return 0;
}
#endif//defined(NEW_DAY)
