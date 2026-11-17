#include <stdio.h>
#include <string.h>
#include <bitset>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"

static const char *TAG = "RANGE";

#define SHTC3_SDA_IO     GPIO_NUM_7
#define SHTC3_SCL_IO     GPIO_NUM_8
#define SR04_TRIG        GPIO_NUM_1
#define SR04_ECHO        GPIO_NUM_0
#define SHTC3_ADDR       0x70
#define I2C_MASTER_FREQ_HZ          CONFIG_I2C_MASTER_FREQUENCY 

static i2c_master_dev_handle_t dev;

/*
* Purpose:      Split 16 bit commands into a high byte and a low byte, 
*               then send command 
*
* Parameters:   cmd:    Command to be sent 
*
* Returns:      esp_err_t 
*/
static esp_err_t shtc3_cmd(uint16_t cmd)
{
    uint8_t buf[2] = { static_cast<uint8_t>(cmd >> 8), 
        static_cast<uint8_t>(cmd & 0xFF) };      // make 2 byte buffer of the MSB and LSB 
    return i2c_master_transmit(dev, buf, 2, 1000);  // transmit the buffer, return error code
}

/*
* Purpose:      Use the checksum byte of the SHTC3 output to check 
*               read results, according to the calculation specified by
*               the SHTC3 datasheet 
*
* Parameters:   *data:  data that will be checked 
*               len:    how many bytes to check from data (likely 2)
*
* Returns:      8-bit crc value that can be used to check reading 
*/
static uint8_t checkSum(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;                         // crc is initialized to all 1's 
    for (size_t i = 0; i < len; i++)            // iterate through bytes 
    {
        crc ^= data[i];                         // XOR crc with ith byte
        for (int j = 0; j < 8; j++)             // iterate through all 8 bits of the byte
        {
            if (crc & 0x80)                     // if crc's first most significant bit is 1
            {
                crc = (crc << 1) ^ 0x31;        // bitshift it left one, and XOR it with 0x31
            }
            else                                // if first bit isn't 1
            {
                crc <<= 1;                      // just shift left 
            }
        }
    }
    return crc; 
}

/*
* Purpose:      Read temperature from SHTC3 sensor, store in address
*               passed by caller
*
* Parameters:   *t:     Pointer to a float that temp will be stored in 
*
* Returns:      esp_err_t 
*/
static esp_err_t shtc3ReadTemp(float *t)
{
    uint8_t d[3];                                   // this is where we will store the 6 byte measurement

    esp_err_t ret = shtc3_cmd(0x7866);              // Read T first, normal mode, no clock stretching
    if (ret != ESP_OK) { ESP_LOGE(TAG, "measure: %s", esp_err_to_name(ret)); shtc3_cmd(0xB098); return ret; }
    vTaskDelay(pdMS_TO_TICKS(20));                  // REALLY IMPORTANT max measurement time is ~12 ms 
   
    ret = i2c_master_receive(dev, d, 3, 1000);      // read 3 bytes from SHTC3 into d array, 1000 Hz
    
    if (checkSum(&d[0], 2) != d[2])                 // if our checksum result doesn't match d[2]
    {
        ESP_LOGE(TAG, "Temp CRC mismatch detected");// print that there's been a mismatch
        return ESP_ERR_INVALID_CRC;                 // exit with error
    }

    uint16_t raw_t  = (d[0] << 8) | d[1];           // bit shift MSB to the left side of 16 bits, concatenate LSB on right side
    *t = -45.0f + 175.0f * raw_t / 65536.0f;        // Temp conversion formula from SHTC Datasheet
    return ESP_OK;                                  // if we made it this far, everything worked and we can send ESP_OK
}

extern "C" void app_main(void)
{
    // configure SHTC3 bus
    i2c_master_bus_config_t bus_cfg1 = {};          // describe the bus 
    bus_cfg1.i2c_port = I2C_NUM_0;                  // esp32c3 only has one I2C controller
    bus_cfg1.sda_io_num = SHTC3_SDA_IO;             // SDA pin 7 
    bus_cfg1.scl_io_num = SHTC3_SCL_IO;             // SCL pin 8 
    bus_cfg1.clk_source = I2C_CLK_SRC_DEFAULT;      // default clock which feeds the SHTC3
    bus_cfg1.glitch_ignore_cnt = 7;                 // this ignores short noise spikes on the lines
    bus_cfg1.flags.enable_internal_pullup = true;   // turn on the weak pull ups 

    i2c_master_bus_handle_t bus1;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg1, &bus1));        // create bus, store in bus handle 

    // probe SHTC3 and LCD, print our ESP_ERR to see if anything malfunctions 
    ESP_LOGI(TAG, "probe 0x70: %s", esp_err_to_name(i2c_master_probe(bus1, SHTC3_ADDR, 100)));

    i2c_device_config_t dev_cfg = {};               // register the device 
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = SHTC3_ADDR;            // input the address (0x70) of the SHTC3
    dev_cfg.scl_speed_hz = 100000;                  // 100kHz

    // add device, error check 
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus1, &dev_cfg, &dev));

    // initialize GPIO output 
    gpio_config_t outputConf = {};
    outputConf.intr_type = GPIO_INTR_DISABLE;       
    outputConf.mode = GPIO_MODE_OUTPUT;
    outputConf.pin_bit_mask = (1ULL << SR04_TRIG);
    outputConf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
    outputConf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&outputConf);

    // initialize GPIO input 
    gpio_config_t inputConf = {};
    inputConf.intr_type = GPIO_INTR_DISABLE;
    inputConf.mode = GPIO_MODE_INPUT;
    inputConf.pin_bit_mask = (1ULL << SR04_ECHO);
    inputConf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
    inputConf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&inputConf); 

    while (1)                                       // main loop 
    {
        float temp;                                 // create floats to hold temp 

        esp_err_t ret = shtc3_cmd(0x3517);          // SHTC3 wakeup command 0x3517
        if (ret != ESP_OK) { ESP_LOGE(TAG, "wakeup: %s", esp_err_to_name(ret)); continue; }     // skip loop if error detected
        vTaskDelay(pdMS_TO_TICKS(10));              // wait before next commands 

        if (shtc3ReadTemp(&temp) == ESP_OK)         // check temp read is fine
        {    
            gpio_set_level(SR04_TRIG, 1);           // trigger the input for the SR04
            esp_rom_delay_us(10);                   // wait 10 microsecond
            gpio_set_level(SR04_TRIG, 0);           // then back off

            // get an initial time, initialize rise and width to zero for goto statements 
            int64_t startTime = esp_timer_get_time(); 
            int64_t rise = 0;
            int64_t width = 0;
            float soundSpeed = 0.0001 * (331 + (0.6 * temp));   // v = 331 m/s + (0.6 * T), adjusted for cm and microseconds
            float distance = 0.; 

            while (gpio_get_level(SR04_ECHO) == 0)  // while there is no response from echo 
            {
                if (esp_timer_get_time() - startTime > 38000)   // sensor stops waiting after 38 ms 
                {
                    ESP_LOGW(TAG, "no rising edge");            // report nothing happened 
                    goto done;                                  // jump and skip loop 
                }
            }

            rise = esp_timer_get_time();

            while (gpio_get_level(SR04_ECHO) == 1)  // while echo is high 
            {
                if (esp_timer_get_time() - rise > 30000)        // sensor stops waiting after 38 ms 
                {
                    ESP_LOGW(TAG, "echo stuck high");           // report signal is stuck high
                    goto done;                                  // jump and skip loop 
                } 
            }
            width = esp_timer_get_time() - rise;    // subtract initial rise time from current time to get width in microseconds
            // ESP_LOGI(TAG, "echo width: %lld us", width);

            distance = (soundSpeed * width) / 2;    // multiply speed by width to get distance in cm 

            ESP_LOGI(TAG, "Distance: %.1f cm at %.0fC", distance, temp); 

            done:; 
        }

        ret = shtc3_cmd(0xB098);                    // back to sleep 
        if (ret != ESP_OK) { ESP_LOGE(TAG, "read: %s", esp_err_to_name(ret)); continue; }       // skip loop if error detected

        vTaskDelay(pdMS_TO_TICKS(1000));            // wait for 1 s (1000 ms), then loop again 
    }
}