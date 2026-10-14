#include <stdio.h>
#include <string.h>
#include <bitset>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "DFRobot_RGBLCD1602.h"

static const char *TAG = "TEMP";

#define SHTC3_SDA_IO     GPIO_NUM_7
#define SHTC3_SCL_IO     GPIO_NUM_8
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

/*
* Purpose:      Read humidity from SHTC3 sensor, store in address
*               passed by caller
*
* Parameters:   *h:     Pointer to a float that humidity will be stored in 
*
* Returns:      esp_err_t 
*/
static esp_err_t shtc3ReadHumidity(float *h)
{
    uint8_t d[3];                                   // this is where we will store the 6 byte measurement

    esp_err_t ret = shtc3_cmd(0x58E0);              // Humidity first, normal mode, no clock stretching
    if (ret != ESP_OK) { ESP_LOGE(TAG, "measure: %s", esp_err_to_name(ret)); shtc3_cmd(0xB098); return ret; }
    vTaskDelay(pdMS_TO_TICKS(20));                  // REALLY IMPORTANT max measurement time is ~12 ms 

    ret = i2c_master_receive(dev, d, 3, 1000);      // read 3 bytes from SHTC3 into d array, 1000 Hz
    
    if (checkSum(&d[0], 2) != d[2])                 // if our checksum result doesn't match d[2]
    {
        ESP_LOGE(TAG, "Humidity CRC mismatch detected");    // print that there's been a mismatch
        return ESP_ERR_INVALID_CRC;                 // exit with error 
    }

    uint16_t rawHumid = (d[0] << 8) | d[1];         // bit shift MSB to the left side of 16 bits, concatenate LSB on right side
    *h = 100.0f * rawHumid / 65536.0f;              // Humidity conversion formula from SHTC datasheet
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
    ESP_LOGI("lcd", "probe 0x3E %s", esp_err_to_name(i2c_master_probe(bus1, 0x3E, 100)));

    i2c_device_config_t dev_cfg = {};               // register the device 
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = SHTC3_ADDR;            // input the address (0x70) of the SHTC3
    dev_cfg.scl_speed_hz = 100000;                  // 100kHz

    // add device, error check 
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus1, &dev_cfg, &dev));
    
    esp_err_t r = i2c_master_probe(bus1, 0x3E, 100); // probe LCD at 0x3E
    ESP_LOGI("lcd", "probe 0x3E: %s", esp_err_to_name(r));

    // configure lcd function, address 0x2D
    DFRobot_RGBLCD1602 lcd(0x2D, 16, 2, bus1);
    lcd.init();

    while (1)                                       // main loop 
    {
        float temp, humidity;                       // create floats to hold temp and umidity 

        esp_err_t ret = shtc3_cmd(0x3517);          // SHTC3 wakeup command 0x3517
        if (ret != ESP_OK) { ESP_LOGE(TAG, "wakeup: %s", esp_err_to_name(ret)); continue; }     // skip loop if error detected
        vTaskDelay(pdMS_TO_TICKS(10));              // wait before next commands 

        if ( (shtc3ReadTemp(&temp) == ESP_OK) && (shtc3ReadHumidity(&humidity) == ESP_OK) )    // check temp and humidity reads are fine
        {    
            // assignment requires ints, not floats, so convert
            int tempRound = static_cast<int>(temp);
            int humRound = static_cast<int>(humidity);
            
            // create strings before we put on screen 
            std::string lineOne = "Temp: " + std::to_string(tempRound);
            lineOne += "C";
            std::string lineTwo = "Hum : " + std::to_string(humRound);
            lineTwo += "%";

            ESP_LOGI(TAG, "Outputting temp and humidity...");
            lcd.setCursor(0, 0);
            lcd.print(lineOne.c_str());
            lcd.setCursor(0, 1);
            lcd.print(lineTwo.c_str());
        }
        ret = shtc3_cmd(0xB098);                    // back to sleep 
        if (ret != ESP_OK) { ESP_LOGE(TAG, "read: %s", esp_err_to_name(ret)); continue; }       // skip loop if error detected

        vTaskDelay(pdMS_TO_TICKS(1000));            // wait for 1 s (1000 ms), then loop again 
    }
}