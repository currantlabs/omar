#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adi_spi.h"
#include "utils.h"


/*
 * adi_3byte_to_int
 * given a 3 byte value from the ADI, convert into an int32_t.
 * buff must contain the value as read from the ADI register, MSB to LSB.
 */
int32_t adi_3byte_to_int(uint8_t *buff)
{
    uint8_t sign;

    if (buff[0] & 0x80) sign = 0xff;
    else sign = 0x00;

    return (sign << 24) + (buff[0] << 16) + (buff[1] << 8) + buff[2];
}

void int_to_adi_3byte(int32_t val, uint8_t *buff)
{
    buff[0] = (val >> 16) & 0xff;
    buff[1] = (val >> 8) & 0xff;
    buff[2] = val & 0xff;
}


void hexdump_bytes(uint8_t *buff, unsigned int len)
{
    hexdump_bytes_log(buff, len);
}

void hexdump_bytes_log(uint8_t *buff, unsigned int len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
		printf("%02x ", buff[i]);
    }

    printf("\n");
}


void hexdump_longs(int32_t *buff, unsigned int len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        printf("%d\n", buff[i]);
        //note: added this so we don't overflow uart when dumping voltage array
        vTaskDelay(100 / portTICK_RATE_MS);
    }

    printf("done\n");
}

void adi_dump_reg(SpiCmdNameT reg)
{
    uint8_t rx_buff[4];
    int rx_len = spi_read_reg(reg, rx_buff);
    printf("%s:\r\n", get_reg_name(reg));
    hexdump_bytes(rx_buff, rx_len);
}


