/* Copyright (c) 2018 Currant Inc. All Rights Reserved.
 *
 * powertest.c - code to run continuously, making omar burn as much power as possible
 */

#if defined(POWERTEST)


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

void powertest(void)
{
	printf("powertest started:\n");

	while (1){
		printf("powertesting...\n");
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}

}

#endif //defined(POWERTEST)
