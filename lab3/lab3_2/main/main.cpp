#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "DFRobot_RGBLCD1602.h"

#define I2C_MASTER_SCL_IO           GPIO_NUM_8                  /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_7                  /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

extern "C" void app_main(void)
{
    // configure master bus 
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;                       // set to I2C master port
    bus_cfg.sda_io_num = I2C_MASTER_SDA_IO;             // set to I2C master SDA
    bus_cfg.scl_io_num = I2C_MASTER_SCL_IO;             // set to I2C master SCL
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;           // set to I2C default clock freq
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;        // enable pullups 

    // create new master bus 
    i2c_master_bus_handle_t bus; 
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    // probe the LCD screen at 0x3E, print result 
    esp_err_t r = i2c_master_probe(bus, 0x3E, 100);
    ESP_LOGI("lcd", "probe 0x3E: %s", esp_err_to_name(r));

    // call constructor 
    DFRobot_RGBLCD1602 lcd(0x2D, 16, 2, bus);

    while (true) {                                      // main loop 
        lcd.init();                                     // initialize screen

        lcd.print("Hello CSE121!");
        lcd.setCursor(0, 1);
        lcd.print("Ngo");
    }
}