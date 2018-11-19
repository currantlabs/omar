/* Copyright (c) 2015 Currant Inc. All Rights Reserved.
 *
 * s5852a.c - routines to control the S5852-A temperature module
 */



#include <stdint.h>
#include <stdbool.h>
#include "i2c.h"
#include "s5852a.h"
#if		defined(NEW_DAY)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "app_error.h"
#include "console.h"
#include "test.h"
#include "log.h"
#include "nrf_delay.h"
#include "wdt.h"
#include "ltime.h"
#include "factory.h"
#endif//defined(NEW_DAY)

#include "esp_err.h"



#define S5852A_I2C_ADDRESS      0x18

#define S5852A_CAP_REG          0x00
#define S5852A_CONF_REG         0x01
#define S5852A_DT_H_REG         0x02
#define S5852A_DT_L_REG         0x03
#define S5852A_ST_H_REG         0x04
#define S5852A_TEMP_REG         0x05
#define S5852A_RES_REG          0x08

#define TICK_TIMER_INTERVAL_MS (5000)

static float raw_to_float(uint8_t raw[2]);
#if		defined(NEW_DAY)
static int console_command(int argc, char *argv[]);
static int test(void);
static int factory_temperature(void);
static int temperature_record(int count);
#endif//defined(NEW_DAY)
static bool m_initialized = false;

#if		defined(NEW_DAY)
static int m_temperature_sample_counter;

APP_TIMER_DEF(m_temperature_sample_timer_id);

static void tick_timer_handler(void * p_context)
{

    if (--m_temperature_sample_counter == 0) {
        uint32_t err_code;
        err_code = app_timer_stop(m_temperature_sample_timer_id);
        APP_ERROR_CHECK(err_code);
        LOG(LOG_LEVEL_DEBUG, "temperature measurement complete!\r\n");
    }

    else {

        float temperature;
        esp_err_t ret = s5852a_get(&temperature);
        if (ret != NRF_SUCCESS) {
            LOG(LOG_LEVEL_DEBUG, "error reading temperature!\r\n");
        } else {
            LOG(LOG_LEVEL_DEBUG, "%s, %f\r\n", time_get_localtime_string(time_get_utc()), temperature);
        }
    }

}
#endif//defined(NEW_DAY)

void s5852a_init(void)
{
    //set pointer to the Ambient Temperature register
    uint8_t pointer = S5852A_TEMP_REG;
    esp_err_t ret = i2c_tx(S5852A_I2C_ADDRESS, &pointer, 1);
    if (ret != ESP_OK) {
        printf("error: can't set s5852 pointer!\n");
        return;
    }

#if		defined(NEW_DAY)
    factory_register_cmd(FACTORY_PREASSY__CHECKTEMP, 
                         FACTORY_FUNC__SIMPLE, 
                         (factory_fptrT ) {.simple_cmd = factory_temperature});

    console_register_cmd("temperature", "temperature related commands", console_command);
    test_register_func("temperature", test);

    uint32_t err_code;

    err_code = app_timer_create(&m_temperature_sample_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                tick_timer_handler);
    APP_ERROR_CHECK(err_code);
#endif//defined(NEW_DAY)

	printf("s5852 successfully initialized\n");
    m_initialized = true;
}


#if		defined(NEW_DAY)
static void usage(void)
{
    LOG(LOG_LEVEL_DEBUG, "usage: temperature  -- dumps current temperature to console\r\n");
    LOG(LOG_LEVEL_DEBUG, "usage: temperature record  -- dumps temperature to console every 30 seconds for 5 minutes\r\n");
    LOG(LOG_LEVEL_DEBUG, "usage: temperature record <count> -- dumps temperature to console every 30 seconds for <count> times\r\n");
}
#endif//defined(NEW_DAY)

/*
 * temperature_get - get current temperature.
 *  returns a floating point value with 0.25C resolution
 */
esp_err_t s5852a_get(float *temperature)
{
    if (!m_initialized) return ESP_FAIL;

    uint8_t pointer = S5852A_TEMP_REG;
    esp_err_t ret = i2c_tx(S5852A_I2C_ADDRESS, &pointer, 1);
    if (ret != ESP_OK) {
        printf("error: can't set s5852 pointer!\n");
        return -1;
    }

    uint8_t raw[2];
    ret = i2c_rx(S5852A_I2C_ADDRESS, raw, 2, false);

	printf("%s(): Raw return values, raw[0] = 0x%02x, raw[1] = 0x%02x\n", __func__, raw[0], raw[1]);
	
    *temperature = raw_to_float(raw);
    return ret;
}

static float raw_to_float(uint8_t raw[2])
{
    //we're using default resolution (10-bit, 0.25C resolution)
    int16_t ambient = (raw[0] << 8) | raw[1];

    ambient <<= 3; //shift the sign bit up to the MSB, so we have a signed value

    float temp = ambient;  //convert to a floating point value

    //divide by 128:
    //  >>3 to undo shift for sign;
    //  >>2 to drop B0 and B1;
    //  >>2 because B2 and B3 are on the right side of the decimal point
    //now LSB is 0.25C
    temp /= 128.0;
    return temp;
}

#if		defined(NEW_DAY)
static int console_command(int argc, char *argv[])
{
    if (argc > 2) {
        usage();
        return 0;
    }

    if (argc == 0) {
        float temperature;
        ret_code_t ret = s5852a_get(&temperature);
        if (ret != ESP_OK) {
            LOG(LOG_LEVEL_DEBUG, "error reading temperature!\r\n");
        } else {
            LOG(LOG_LEVEL_DEBUG, "current temperature: %u\r\n", (unsigned int)temperature);
        }
        return 0;
    }

    if (argc == 1 && strcmp("record", argv[0]) == 0) {
        temperature_record(0);
        return 0;
    }

    else if (argc == 2 && strcmp("record", argv[0]) == 0) {
        uint32_t count = strtoul(argv[1], (char **)NULL, 0);  //base 10 or 16 (with 0x)
        temperature_record(count);
        return 0;
    }


    else {
        usage();
        return 0;
    }


    return 0;
}

#define ROOM_TEMPERATURE_MIN (15) //59F
#define ROOM_TEMPERATURE_MAX (35) //95F
static int test(void)
{
    float temperature;
    LOG(LOG_LEVEL_DEBUG, "reading temperature: ");
    ret_code_t ret = s5852a_get(&temperature);
    if (ret != NRF_SUCCESS) {
        LOG(LOG_LEVEL_DEBUG, "[FAILED]\r\n");
        return -1;
    } else {
        if ((temperature < ROOM_TEMPERATURE_MIN) || (temperature > ROOM_TEMPERATURE_MAX)) {
            LOG(LOG_LEVEL_DEBUG, "[FAILED] (%u C)\r\n", (unsigned int)temperature);
            return -1;
        } else {
            LOG(LOG_LEVEL_DEBUG, "[OK]\r\n");
        }
    }

    uint8_t vectors[][2] = {
        {0xe7, 0xd0},  //+125.00
        {0xe5, 0x50},  //+85.00
        {0xe4, 0x10},  //+65.00
        {0xe1, 0x90},  //+25.00
        {0xe0, 0x10},  //+1.00
        {0xe0, 0x04},  //+0.25
        {0xe0, 0x00},  //+0.00
        {0xff, 0xfc},  //-0.25
        {0xff, 0xf0},  //-1.00
        {0xfe, 0xc0},  //-20.00
        {0xfd, 0x80},  //-40.00
    };

    for (int i=0; i<sizeof(vectors)/sizeof(int16_t); i++) {
        float result = raw_to_float(vectors[i]);
        LOG(LOG_LEVEL_DEBUG, "vector %d: %0.2f\r\n", i, result);
    }

    return 0;
}

static int temperature_record(int count)
{
    if (count == 0) count = 24;

    m_temperature_sample_counter = count;
    LOG(LOG_LEVEL_DEBUG, "taking %d measurements...\r\n", m_temperature_sample_counter);

    uint32_t err_code = app_timer_start(m_temperature_sample_timer_id, BSP_MS_TO_TICK(TICK_TIMER_INTERVAL_MS), NULL);
    APP_ERROR_CHECK(err_code);

    return 0;
}

static int factory_temperature(void)
{
    float temperature;
    ret_code_t ret = s5852a_get(&temperature);

    if (ret != NRF_SUCCESS) 
    {
        LOG(LOG_LEVEL_DEBUG, "error reading temperature!\r\n");
        return FACTORY_TEST_STATUS__FAILED;
    } 

    LOG(LOG_LEVEL_DEBUG, "current temperature: %u\r\n", (unsigned int)temperature);

    return FACTORY_TEST_STATUS__PASSED;

}
#endif//defined(NEW_DAY)
