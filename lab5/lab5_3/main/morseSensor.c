#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"

static const char *TAG = "morseSensor";

#define PD_CHANNEL ADC_CHANNEL_3      // GPIO3 on the ESP32-C3
#define UNIT_US 2000LL                // morse time unit = 8 ms
#define SAMPLES 20                    // 20 Samples = 0.772 ms
#define THRESHOLD_HI 14               // above this raw number, the led is on 
#define THRESHOLD_LO 11               // above this raw number, the led is off 

static const char *MORSE[36] = 
{
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..",               // A-Z
    "-----", ".----", "..---", "...--", "....-",
    ".....", "-....", "--...", "---..", "----."                 // 0-9
};

static char decode(const char *str)
{
    for (int i = 0; i < 36; i++)                                // iterate through MORSE array 
    {
        if (strcmp(str, MORSE[i]) == 0)                         // if match 
        {
            if (i < 26) { return ('A' + i); }                   // return A + i if letter
            else { return '0' + (i - 26); }                     // if number, return 0 + (i - 26)
        }
    }

    return '?';
}

static int read_avg(adc_oneshot_unit_handle_t h, adc_channel_t ch)
{
    int sum = 0;
    for (int i = 0; i < SAMPLES; i++)
    {
        int raw; 
        ESP_ERROR_CHECK(adc_oneshot_read(h, ch, &raw));
        sum += raw; 
    }

    return sum / SAMPLES;                   
}

void app_main(void)
{
    // Create the ADC unit
    adc_oneshot_unit_handle_t adc_handle;                       // adc handle 
    adc_oneshot_unit_init_cfg_t unit_cfg =                      // configure adc handle 
    {    
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // Configure the channel
    adc_oneshot_chan_cfg_t chan_cfg = 
    {
        .atten = ADC_ATTEN_DB_12,                               // measure roughly 0 to 3.1 V
        .bitwidth = ADC_BITWIDTH_DEFAULT,                       // 12 bits for measurement: values 0-4095
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, PD_CHANNEL, &chan_cfg));

    // Read in a loop
    
    int codeLen = 0;                                            // this is needed to keep tack of how long our code letter is 
    bool prevOn = false;                                        // keep track of if light was on last tick 
    bool letterDone = true;                                     // keep track of if letter is done-start with yes
    bool wordDone = true;                                       // keep track of if word is done-start with yes 
    char currCode[8] = "";
    int64_t lastEdge = esp_timer_get_time();                    // we will track how long the light is on/off with ESP timer 

    while (1) 
    {
        // int64_t t0 = esp_timer_get_time(); 
        int val = read_avg(adc_handle, PD_CHANNEL);
        // ESP_LOGI(TAG, "avg=%d took %" PRId64 " us", val, esp_timer_get_time() - t0);
        // ESP_LOGI(TAG, "avg = %d", val);

        bool on = prevOn;
        if (val > THRESHOLD_HI)      on = true;
        else if (val < THRESHOLD_LO) on = false;
        // ESP_LOGI(TAG, "%s", (on) ? "on" : "off");

        int64_t currTime = esp_timer_get_time();                // get current time 
        // ESP_LOGI(TAG, "currTime : %lld", (long long)currTime);
        int64_t dur = currTime - lastEdge;                      // set duration to the difference between now and last edge 

        if (on != prevOn)                                       // if mismatch between on and preOn, an edge has occured 
        {
            if (!on)                                            // if negative edge
            {
                if ( (dur > UNIT_US / 3) && (codeLen < 6) )     // if pos edge was greater than unit/3, and code isn't completed
                {
                    // less than 2 time units is a dot. greater than 2 is a dash 
                    // also, increment codeLen 
                    currCode[codeLen++] = (dur < 2 * UNIT_US) ? '.' : '-';
                    currCode[codeLen] = '\0';                   // add end character 
                }
                letterDone = false;                             // letter isn't done yet 
                wordDone = false;                               // word isn't done yet either 
            }
            prevOn = on;                                        // prevOn now takes the value of on 
            lastEdge = currTime;                                // lastEdge is now currTime 
        }
        else if (!on)                                           // if we are still off (even if edge hasn't occurred)
        {
            if (!letterDone && (dur > 2 * UNIT_US))             // if there has been a gap longer than 2 units, letter has ended
            {
                if (codeLen > 0)                                // check that something was written to currCode
                {
                    printf("%c", decode(currCode));             // print decoded currCode
                    fflush(stdout);
                }
                currCode[0] = '\0';                             // clear currCode 
                codeLen = 0;                                    // reset codeLen to 0 
                letterDone = true;                              // letter is now done 
            }
            if (!wordDone && (dur > 5 * UNIT_US))               // gap longer than 5 unites means word has ended 
            {
                printf(" \n");                                  // print a space 
                fflush(stdout);
                wordDone = true;                                // word is now done 
            }
        }

        esp_rom_delay_us(50); 
    }
}