#pragma once


void hexdump_bytes(uint8_t *buff, unsigned int len);
void hexdump_bytes_log(uint8_t *buff, unsigned int len);
void hexdump_longs(int32_t *buff, unsigned int len);
void adi_dump_reg(SpiCmdNameT reg);

int32_t adi_3byte_to_int(uint8_t *buff);
void int_to_adi_3byte(int32_t val, uint8_t *buff);
