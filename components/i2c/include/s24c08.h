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
 * Block0:	1010000b (0x50)
 * Block1:	1010001b (0x51)
 * Block2:	1010010b (0x52)
 * Block3:	1010011b (0x53)
 * 
 */

#define S24C08C_I2C_ADDRESS_BLOCK0      0x50
#define S24C08C_I2C_ADDRESS_BLOCK1      0x51
#define S24C08C_I2C_ADDRESS_BLOCK2      0x52
#define S24C08C_I2C_ADDRESS_BLOCK3      0x53
