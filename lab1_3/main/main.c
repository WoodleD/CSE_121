/*
 * SPDX-FileCopyrightText: 2010-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BLINK_GPIO 2						// we are flashing GPIO 2

void blink_task(void *pvParameters)
{
	gpio_reset_pin(BLINK_GPIO);				// reset GPIO pin
	gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);	// set direction as the output 

	while (1)
	{
		// turn GPIO on 
		gpio_set_level(BLINK_GPIO, 1);
		vTaskDelay(pdMS_TO_TICKS(1000));

		// turn GPIO off
		gpio_set_level(BLINK_GPIO, 0); 	
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void app_main(void)
{	
	xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);
}

