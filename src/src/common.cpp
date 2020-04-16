#include "common.h"
#include "LoRaRadioLib.h"
#include "utils.h"
#include <Arduino.h>

// TODO: Validate values for RFmodeCycleAddtionalTime and RFmodeCycleInterval for rates lower than 50HZ

const expresslrs_mod_settings_s ExpressLRS_AirRateConfig[RATE_MAX] = {
    {BW_500_00_KHZ, SF_6, CR_4_5, -112, 5000, 200, TLM_RATIO_1_64, 4, 8, RATE_200HZ, 1000, 1500},
    {BW_500_00_KHZ, SF_7, CR_4_7, -117, 10000, 100, TLM_RATIO_1_32, 4, 8, RATE_100HZ, 2000, 2000},
    {BW_500_00_KHZ, SF_8, CR_4_7, -120, 20000, 50, TLM_RATIO_1_16, 2, 10, RATE_50HZ, 6000, 2500},
#if RATE_MAX > 3
    {BW_250_00_KHZ, SF_8, CR_4_7, -123, 40000, 25, TLM_RATIO_NO_TLM, 2, 8, RATE_25HZ, 6000, 2500}, // not using thse slower rates for now
#elif RATE_MAX > 4
    {BW_250_00_KHZ, SF_11, CR_4_5, -131, 250000, 4, TLM_RATIO_NO_TLM, 2, 8, RATE_4HZ, 6000, 2500},
#endif
};

const expresslrs_mod_settings_s *get_elrs_airRateConfig(uint8_t rate)
{
    if (RATE_MAX <= rate)
        rate = (RATE_MAX - 1);
    return &ExpressLRS_AirRateConfig[rate];
}

volatile const expresslrs_mod_settings_s *ExpressLRS_currAirRate = NULL;

#ifndef MY_UID
//uint8_t UID[6] = {48, 174, 164, 200, 100, 50};
//uint8_t UID[6] = {180, 230, 45, 152, 126, 65}; //sandro unique ID
uint8_t UID[6] = {180, 230, 45, 152, 125, 173}; // Wez's unique ID
#else
uint8_t UID[6] = {MY_UID};
#endif

uint8_t CRCCaesarCipher = UID[4];
uint8_t DeviceAddr = UID[5] & 0b111111; // temporarily based on mac until listen before assigning method merged

#define RSSI_FLOOR_NUM_READS 5 // number of times to sweep the noise foor to get avg. RSSI reading
#define MEDIAN_SIZE 20

int16_t MeasureNoiseFloor()
{
    int NUM_READS = RSSI_FLOOR_NUM_READS * NR_FHSS_ENTRIES;
    int32_t returnval = 0;

    for (uint32_t freq = 0; freq < NR_FHSS_ENTRIES; freq++)
    {
        FHSSsetCurrIndex(freq);
        Radio.SetMode(SX127X_CAD);

        for (int i = 0; i < RSSI_FLOOR_NUM_READS; i++)
        {
            returnval += Radio.GetCurrRSSI();
            delay(5);
        }
    }
    returnval /= NUM_READS;
    return (returnval);
}

uint16_t TLMratioEnumToValue(uint8_t enumval)
{
#if 0
    switch (enumval)
    {
    case TLM_RATIO_1_2:
        return 2;
        break;
    case TLM_RATIO_1_4:
        return 4;
        break;
    case TLM_RATIO_1_8:
        return 8;
        break;
    case TLM_RATIO_1_16:
        return 16;
        break;
    case TLM_RATIO_1_32:
        return 32;
        break;
    case TLM_RATIO_1_64:
        return 64;
        break;
    case TLM_RATIO_1_128:
        return 128;
        break;
    case TLM_RATIO_NO_TLM:
    default:
        return 0xFF;
    }
#else
    return (256u >> (enumval));
#endif
}

static bool ledState = false;
void led_set_state(bool state)
{
    ledState = state;
    platform_set_led(state);
}

void led_toggle(void)
{
    led_set_state(!ledState);
}
