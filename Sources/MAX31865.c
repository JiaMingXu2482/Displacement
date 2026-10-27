#include "MAX31865.h"
#include "SPI.h"
#include "osif.h"
#include <stdbool.h>
#include <stddef.h>

/* MAX31865 datasheet: registers 0x00..0x07; bit 7 of address selects write. */
#define REG_CONFIG       0x00U
#define REG_RTD_MSB      0x01U
#define REG_HIGH_MSB     0x03U
#define REG_FAULT        0x07U
#define CONFIG_BIAS      0x80U
#define CONFIG_AUTO      0x40U
#define CONFIG_ONE_SHOT  0x20U
#define CONFIG_3WIRE     0x10U
#define CONFIG_CLEAR     0x02U
#define CONFIG_FAULT_STEP1 0x08U
#define CONFIG_FAULT_STEP2 0x0CU
#define CONFIG_50HZ      0x01U
#define CONFIG_IDLE      (CONFIG_3WIRE | CONFIG_50HZ)
#define CONFIG_CONTINUOUS (CONFIG_IDLE | CONFIG_BIAS | CONFIG_AUTO)

static float rrefOhms;
static bool initialized;
static max31865_config_diagnostic_t configDiagnostic;

max31865_config_diagnostic_t MAX31865_GetConfigDiagnostic(void)
{
    return configDiagnostic;
}

static max31865_status_t WriteRegister(uint8_t address, uint8_t value)
{
    const uint8_t tx[2] = {(uint8_t)(address | 0x80U), value};
    uint8_t rx[2];
    return SPI_Transfer(tx, rx, 2U) == STATUS_SUCCESS ?
           MAX31865_OK : MAX31865_SPI_ERROR;
}

static max31865_status_t ReadRegisters(uint8_t address, uint8_t *data,
                                      uint16_t length)
{
    uint8_t tx[3] = {address, 0U, 0U};
    uint8_t rx[3] = {0U, 0U, 0U};
    uint16_t i;
    if (length == 0U || length > 2U || data == NULL) {
        return MAX31865_INVALID_ARGUMENT;
    }
    if (SPI_Transfer(tx, rx, (uint16_t)(length + 1U)) != STATUS_SUCCESS) {
        return MAX31865_SPI_ERROR;
    }
    /* The first received byte belongs to the address phase and is discarded. */
    for (i = 0U; i < length; i++) {
        data[i] = rx[i + 1U];
    }
    return MAX31865_OK;
}

static float Pt100Resistance(float temperature)
{
    const float a = 3.9083e-3f;
    const float b = -5.775e-7f;
    const float c = -4.18301e-12f;
    float ratio = 1.0f + a * temperature + b * temperature * temperature;
    if (temperature < 0.0f) {
        ratio += c * (temperature - 100.0f) * temperature * temperature * temperature;
    }
    return 100.0f * ratio;
}

max31865_status_t MAX31865_RawToCelsius(uint16_t raw, float referenceOhms,
                                      int32_t *temperatureCentiC)
{
    float resistance, low = -200.0f, high = 850.0f, temperature, scaled;
    float halfLsb;
    uint8_t i;
    /* Also rejects NaN/Inf, a PT1000 RREF, and disconnected/shorted ADC rails. */
    if (temperatureCentiC == NULL || !(referenceOhms >= 350.0f && referenceOhms <= 1000.0f)) {
        return MAX31865_INVALID_ARGUMENT;
    }
    if (raw == 0U || raw >= 32767U) {
        return MAX31865_OUT_OF_RANGE;
    }
    resistance = (float)raw * referenceOhms / 32768.0f;
    halfLsb = referenceOhms / 65536.0f;
    if (resistance < Pt100Resistance(low) - halfLsb ||
        resistance > Pt100Resistance(high) + halfLsb) {
        return MAX31865_OUT_OF_RANGE;
    }
    /* Invert the monotonic Callendar-Van Dusen equation without libm/sqrtf. */
    for (i = 0U; i < 24U; i++) {
        temperature = (low + high) * 0.5f;
        if (Pt100Resistance(temperature) < resistance) {
            low = temperature;
        } else {
            high = temperature;
        }
    }
    scaled = (low + high) * 50.0f;
    *temperatureCentiC = (int32_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
    return MAX31865_OK;
}

max31865_status_t MAX31865_Init(float referenceOhms)
{
    uint8_t config;
    /* Broad thresholds; physical PT100 range is checked during conversion. */
    const uint8_t thresholds[5] = {REG_HIGH_MSB | 0x80U, 0xFFU, 0xFFU, 0U, 0U};
    uint8_t rx[5];
    initialized = false;
    if (!(referenceOhms >= 350.0f && referenceOhms <= 1000.0f)) {
        return MAX31865_INVALID_ARGUMENT;
    }
    if (SPI_Init() != STATUS_SUCCESS ||
        WriteRegister(REG_CONFIG, CONFIG_CONTINUOUS | CONFIG_CLEAR) != MAX31865_OK ||
        SPI_Transfer(thresholds, rx, 5U) != STATUS_SUCCESS) {
        return MAX31865_SPI_ERROR;
    }
    /* 50 Hz continuous conversion: allow the first result to complete. */
    OSIF_TimeDelay(70U);
    if (ReadRegisters(REG_CONFIG, &config, 1U) != MAX31865_OK) {
        return MAX31865_SPI_ERROR;
    }
    /* MAX31865 has no chip-ID register. Read-back detects a stuck MISO line. */
    configDiagnostic.actual = config;
    configDiagnostic.expected = CONFIG_CONTINUOUS;
    if (config != CONFIG_CONTINUOUS) {
        return MAX31865_CONFIG_ERROR;
    }
    rrefOhms = referenceOhms;
    initialized = true;
    return MAX31865_OK;
}

max31865_status_t MAX31865_ReadTemperature(max31865_measurement_t *measurement)
{
    uint8_t msb, lsb, config;
    uint16_t rtdRegister;
    max31865_status_t result;
    if (measurement == NULL) {
        return MAX31865_INVALID_ARGUMENT;
    }
    measurement->temperatureCentiC = 0;
    measurement->rawRtd = 0U;
    measurement->rtdRegister = 0U;
    measurement->config = 0U;
    measurement->fault = 0U;
    if (!initialized) {
        return MAX31865_NOT_READY;
    }

    result = ReadRegisters(REG_CONFIG, &config, 1U);
    if (result == MAX31865_OK) {
        measurement->config = config;
        configDiagnostic.actual = config;
        configDiagnostic.expected = CONFIG_CONTINUOUS;
        if (config != CONFIG_CONTINUOUS) {
            result = MAX31865_CONFIG_ERROR;
        }
    }
    if (result == MAX31865_OK) {
        /* Read the RTD bytes in separate transactions for robust LPSPI framing. */
        result = ReadRegisters(REG_RTD_MSB, &msb, 1U);
        if (result == MAX31865_OK) {
            result = ReadRegisters((uint8_t)(REG_RTD_MSB + 1U), &lsb, 1U);
        }
        if (result == MAX31865_OK) {
            rtdRegister = ((uint16_t)msb << 8U) | lsb;
            measurement->rtdRegister = rtdRegister;
            measurement->rawRtd = rtdRegister >> 1U;
            result = ReadRegisters(REG_FAULT, &measurement->fault, 1U);
            if (result == MAX31865_OK) {
                if ((rtdRegister & 1U) != 0U || measurement->fault != 0U) {
                    result = MAX31865_RTD_FAULT;
                } else {
                    result = MAX31865_RawToCelsius(measurement->rawRtd, rrefOhms,
                                                 &measurement->temperatureCentiC);
                }
            }
        }
    }
    return result;
}

max31865_status_t MAX31865_RunFaultDetection(uint8_t *fault)
{
    max31865_status_t result, powerDownResult;
    uint8_t config;
    if (fault == NULL) {
        return MAX31865_INVALID_ARGUMENT;
    }
    *fault = 0U;
    if (!initialized) {
        return MAX31865_NOT_READY;
    }

    /* Datasheet manual cycle: bias/clear -> 10b -> wait -> 11b -> wait.
     * No one-shot command during fault detection. 10 ms allows RC settling. */
    result = WriteRegister(REG_CONFIG, CONFIG_IDLE | CONFIG_BIAS | CONFIG_CLEAR);
    if (result == MAX31865_OK) {
        OSIF_TimeDelay(10U);
        result = WriteRegister(REG_CONFIG, CONFIG_IDLE | CONFIG_BIAS | CONFIG_FAULT_STEP1);
    }
    if (result == MAX31865_OK) {
        OSIF_TimeDelay(10U);
        result = WriteRegister(REG_CONFIG, CONFIG_IDLE | CONFIG_BIAS | CONFIG_FAULT_STEP2);
    }
    if (result == MAX31865_OK) {
        OSIF_TimeDelay(10U);
        result = ReadRegisters(REG_CONFIG, &config, 1U);
        if (result == MAX31865_OK) {
            configDiagnostic.actual = config;
            configDiagnostic.expected = CONFIG_IDLE | CONFIG_BIAS;
            if (config != (CONFIG_IDLE | CONFIG_BIAS)) {
                result = MAX31865_CONFIG_ERROR;
            } else {
                result = ReadRegisters(REG_FAULT, fault, 1U);
            }
        }
    }
    powerDownResult = WriteRegister(REG_CONFIG, CONFIG_IDLE);
    return result == MAX31865_OK ? powerDownResult : result;
}
