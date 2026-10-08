#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "SHTC3";

#define SDA_IO     7
#define SCL_IO     8
#define SHTC3_ADDR 0x70

static i2c_master_dev_handle_t dev;

static esp_err_t shtc3_cmd(uint16_t cmd)
{
    uint8_t buf[2] = { cmd >> 8, cmd & 0xFF };
    return i2c_master_transmit(dev, buf, 2, 1000);
}

static esp_err_t shtc3_read(float *temp_c, float *rh)
{
    uint8_t d[6];

    esp_err_t ret = shtc3_cmd(0x3517);              // wakeup
    if (ret != ESP_OK) { ESP_LOGE(TAG, "wakeup: %s", esp_err_to_name(ret)); return ret; }
    vTaskDelay(pdMS_TO_TICKS(10));

    ret = shtc3_cmd(0x7866);                        // T first, normal mode, no clock stretching
    if (ret != ESP_OK) { ESP_LOGE(TAG, "measure: %s", esp_err_to_name(ret)); shtc3_cmd(0xB098); return ret; }
    vTaskDelay(pdMS_TO_TICKS(20));                  // max measurement time is ~12 ms

    ret = i2c_master_receive(dev, d, 6, 1000);
    shtc3_cmd(0xB098);                              // back to sleep
    if (ret != ESP_OK) { ESP_LOGE(TAG, "read: %s", esp_err_to_name(ret)); return ret; }

    uint16_t raw_t  = (d[0] << 8) | d[1];
    uint16_t raw_rh = (d[3] << 8) | d[4];
    *temp_c = -45.0f + 175.0f * raw_t / 65536.0f;
    *rh     = 100.0f * raw_rh / 65536.0f;
    return ESP_OK;
}

void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SDA_IO,
        .scl_io_num = SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    ESP_LOGI(TAG, "probe 0x70: %s", esp_err_to_name(i2c_master_probe(bus, SHTC3_ADDR, 100)));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

    while (1)
    {
        float t, rh;
        if (shtc3_read(&t, &rh) == ESP_OK)
            ESP_LOGI(TAG, "Temperature: %.1f C (%.1f F), humidity: %.1f %%", t, t * 9 / 5 + 32, rh);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}