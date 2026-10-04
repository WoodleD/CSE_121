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
#include "led_strip.h"

#define BLINK_GPIO GPIO_NUM_2
static led_strip_handle_t led_strip;

void blink_task(void *pyParameter)
{
	while (1)
	{
		// turn LED on 
		led_strip_set_pixel(led_strip, 0, 16, 16, 16);
		led_strip_refresh(led_strip);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		
		// turn LED off
		led_strip_clear(led_strip); 
		vTaskDelay(1000 / portTICK_PERIOD_MS);	
	}
}

void app_main(void)
{
	// initialize led strip configuration
	led_strip_config_t strip_config = {
        	.strip_gpio_num = BLINK_GPIO,
        	.max_leds = 1, // at least one LED on board
    	};

	// RMT backend configuration 
	led_strip_rmt_config_t rmt_config = {
        	.resolution_hz = 10 * 1000 * 1000, // 10MHz
        	.flags.with_dma = false,
	};

	// allocate and initialize driver 
	ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
	
	xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL); 
}
