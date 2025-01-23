#include "targets.h"
#include "common.h"
#include "device.h"
#include "logging.h"
#include "POWERMGNT.h"
#include "devADC.h"

#if defined(GPIO_PIN_PA_PDET)

#if defined(USE_SKY85321)
#define SKY85321_MAX_DBM_INPUT 5

// SKY85321_PDET_SLOPE/INTERCEPT convert mV from analogRead() to dBm
#if !defined(SKY85321_PDET_SLOPE) || !defined(SKY85321_PDET_INTERCEPT)
  #error "SKY85321 requires SKY85321_PDET_SLOPE and SKY85321_PDET_INTERCEPT"
#endif
#endif // USE_SKY85321

typedef uint32_t pdet_storage_t;
#define PDET_DBM_SCALE(x)         ((pdet_storage_t)((x) * 1000U))
#define PDET_MV_SCALE(x)          ((pdet_storage_t)((x) * 10U))
#define PDET_MV_DESCALE(x)        ((pdet_storage_t)((x) / 10U))
#define PDET_HYSTERESIS_DBMSCALED PDET_DBM_SCALE(0.7)
#define PDET_SAMPLE_PERIODMS      1000

extern bool busyTransmitting;
static pdet_storage_t PdetMvScaled;
static uint8_t lastTargetPowerdBm;

static int start()
{
    if (GPIO_PIN_PA_PDET != UNDEF_PIN)
    {
        analogSetPinAttenuation(GPIO_PIN_PA_PDET, ADC_0db);
        return DURATION_IMMEDIATELY;
    }
    return DURATION_NEVER;
}

/**
 * @brief Event callback for PDET device.
 *
 * If the module is running in the "normal" mode, i.e. with the radio outputting RF,
 * then set the initial duration to immediately call the 'timeout' function.
 * Otherwise set the duration to never call the power detection/adjustment 'timeout' function.
 *
 * @return int duration in ms to call the 'timeout' function
 */
static int event()
{
    if (GPIO_PIN_PA_PDET == UNDEF_PIN || connectionState > connectionState_e::MODE_STATES)
    {
        return DURATION_NEVER;
    }
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    pdet_storage_t newPdetScaled = PDET_MV_SCALE(getADCReading(ADC_PA_PDET));

    uint8_t targetPowerDbm = POWERMGNT::getPowerIndBm();
    if (PdetMvScaled == 0 || lastTargetPowerdBm != targetPowerDbm)
    {
        PdetMvScaled = newPdetScaled;
        lastTargetPowerdBm = targetPowerDbm;
    }
    else
    {
        PdetMvScaled = (PdetMvScaled * 9 + newPdetScaled) / 10; // IIR filter
    }

    pdet_storage_t dBmScaled = PDET_MV_DESCALE(PDET_DBM_SCALE(SKY85321_PDET_SLOPE) * PdetMvScaled) + PDET_DBM_SCALE(SKY85321_PDET_INTERCEPT);
    pdet_storage_t targetPowerDbmScaled = PDET_DBM_SCALE(targetPowerDbm);
    DBGVLN("PdetMv=%u dBm=%u", PdetMvScaled, dBmScaled);

    if (dBmScaled < (targetPowerDbmScaled - PDET_HYSTERESIS_DBMSCALED) && POWERMGNT::currentSX1280Output() < SKY85321_MAX_DBM_INPUT)
    {
        POWERMGNT::incSX1280Output();
        PdetMvScaled = 0;
    }
    else if (dBmScaled > (targetPowerDbmScaled + PDET_HYSTERESIS_DBMSCALED))
    {
        POWERMGNT::decSX1280Output();
        PdetMvScaled = 0;
    }

    return PDET_SAMPLE_PERIODMS;
}

device_t PDET_device = {
    .initialize = NULL,
    .start = start,
    .event = event,
    .timeout = timeout
};
#endif