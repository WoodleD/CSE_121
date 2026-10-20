/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           GPIO_NUM_8                  /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_7                  /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define ICM_ADDR                    0x68        /*!< Address of the ICM-42670-P sensor */
#define ICM42670_WHO_AM_I_REG       0x75        /*!< Register addresses of the "who am I" register */
#define ICM42670_WHO_AM_I_VAL       0x67        /*!< Register address returneb by "who am I" */
#define ICM42670_PWR_MGMT0_REG      0x1F        /*!< Register addresses of the power management register */
#define MPU9250_RESET_BIT           7

#define ACCEL_SENS_LSB_PER_G        2048.0f   /* +/-16g default */

/**
 * @brief Read a sequence of bytes from a ICM sensor registers
 */
static esp_err_t ICM_register_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief Write a byte to a ICM sensor register
 */
static esp_err_t ICM_register_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

/**
 * @brief i2c master initialization
 */
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ICM_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

void app_main(void)
{
    uint8_t data[6];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    /* Read the ICM WHO_AM_I register, on power up the register should have the value 0x71 */
    ESP_ERROR_CHECK(ICM_register_read(dev_handle, ICM42670_WHO_AM_I_REG, data, 1));
    ESP_LOGI(TAG, "WHO_AM_I = %X", data[0]);

    /* Demonstrate writing by resetting the ICM */
    ESP_ERROR_CHECK(ICM_register_write_byte(dev_handle, ICM42670_PWR_MGMT0_REG, 0x0F));
    vTaskDelay(pdMS_TO_TICKS(15));

    while (1)
    {
        ESP_ERROR_CHECK(ICM_register_read(dev_handle, 0x0B, data, 6));

        // concatenate our bytes into signed 16 bits 
        int16_t rawX = (int16_t)((data[0] << 8) | data[1]);
        int16_t rawY = (int16_t)((data[2] << 8) | data[3]);

        float x = rawX / ACCEL_SENS_LSB_PER_G; 
        float y = rawY / ACCEL_SENS_LSB_PER_G; 

        if (x > 0.05)
        {
            ESP_LOGI(TAG, "LEFT");
        }
        else if (x < -0.05)
        {
            ESP_LOGI(TAG, "RIGHT");
        }

        if (y > 0.05)
        {
            ESP_LOGI(TAG, "UP");
        }
        else if (y < -0.05)
        {
            ESP_LOGI(TAG, "DOWN");
        }

        // ESP_LOGI(TAG, "X axis: %.4f", x);
        // ESP_LOGI(TAG, "Y axis: %.4f", y);
        vTaskDelay(pdMS_TO_TICKS(1000));            // wait for 1 s, look again
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    ESP_LOGI(TAG, "I2C de-initialized successfully");
}
