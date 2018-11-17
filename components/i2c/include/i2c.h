#ifndef I2C_H
#define I2C_H

#include "esp_err.h"
#include <stdint.h>

#define ACK_CHECK_EN	(0x1)              /*!< I2C master will check ack from slave*/


void i2c_init(void);
esp_err_t i2c_tx(uint8_t address, uint8_t* data_wr, size_t size);
esp_err_t i2c_rx(uint8_t address, uint8_t *p_data, uint32_t length, bool xfer_pending);

#endif //I2C_H
