/* Host tests: real sensor driver, simulated register-level SPI device. */
#include "MAX31865.h"
#include "SPI.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint8_t registers[8];
static int stuckMiso = -1;
static bool spiFailure, conversionStuck;
static unsigned failRtdRead;
static uint8_t injectedFault;
static uint8_t injectedCycleFault;
static bool faultCycleStuck;
static uint16_t injectedRaw = 8362U;

status_t SPI_Init(void) { return spiFailure ? STATUS_ERROR : STATUS_SUCCESS; }

status_t SPI_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    unsigned address = tx[0] & 0x7FU;
    unsigned i;
    assert(length >= 2U && address + length - 2U < sizeof(registers));
    if (spiFailure || (address == 1U && failRtdRead > 0U && failRtdRead-- == 1U)) {
        return STATUS_TIMEOUT;
    }
    memset(rx, 0, length);
    for (i = 1U; i < length; ++i) {
        if (tx[0] & 0x80U) {
            registers[address] = tx[i];
            if (address == 0U) {
                if (tx[i] & 0x02U) { registers[7] = 0U; }
                registers[0] &= (uint8_t)~0x02U;
            }
        } else {
            rx[i] = stuckMiso >= 0 ? (uint8_t)stuckMiso : registers[address];
        }
        address++;
    }
    return STATUS_SUCCESS;
}

void OSIF_TimeDelay(uint32_t milliseconds)
{
    if (milliseconds >= 10U && (registers[0] & 0x0CU) == 0x0CU && !faultCycleStuck) {
        registers[0] &= (uint8_t)~0x0CU;
        registers[7] = injectedCycleFault;
    }
    if (milliseconds >= 66U && !conversionStuck && (registers[0] & 0x20U)) {
        uint16_t word = (uint16_t)((injectedRaw << 1U) | (injectedFault ? 1U : 0U));
        registers[0] &= (uint8_t)~0x20U;
        registers[1] = (uint8_t)(word >> 8U);
        registers[2] = (uint8_t)word;
        registers[7] = injectedFault;
    }
}

int main(void)
{
    /* Independent IEC PT100 reference points, allowing ADC quantization. */
    const struct { double resistance; int32_t centiC; } cases[] = {
        {18.52008, -20000}, {60.25584, -10000}, {100.0, 0},
        {109.73465625, 2500}, {138.5055, 10000},
        {212.0515, 30000}, {390.481125, 85000}
    };
    unsigned i;
    int32_t temperature;
    max31865_measurement_t measurement;
    uint8_t cycleFault;
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint16_t raw = (uint16_t)(cases[i].resistance / 430.0 * 32768.0 + 0.5);
        assert(MAX31865_RawToCelsius(raw, 430.0f, &temperature) == MAX31865_OK);
        assert(labs(temperature - cases[i].centiC) <= 4);
    }
    assert(MAX31865_RawToCelsius(0U, 430.0f, &temperature) == MAX31865_OUT_OF_RANGE);
    assert(MAX31865_RawToCelsius(32767U, 430.0f, &temperature) == MAX31865_OUT_OF_RANGE);
    assert(MAX31865_RawToCelsius(500U, 430.0f, &temperature) == MAX31865_OUT_OF_RANGE);
    assert(MAX31865_RawToCelsius(8000U, NAN, &temperature) == MAX31865_INVALID_ARGUMENT);
    assert(MAX31865_RawToCelsius(8000U, INFINITY, &temperature) == MAX31865_INVALID_ARGUMENT);
    assert(MAX31865_RawToCelsius(8000U, 430.0f, NULL) == MAX31865_INVALID_ARGUMENT);
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_NOT_READY);
    assert(MAX31865_RunFaultDetection(&cycleFault) == MAX31865_NOT_READY);
    assert(MAX31865_Init(430.0f) == MAX31865_OK);
    assert(registers[0] == 0x11U); /* Three wire, 50 Hz, bias off. */
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_OK);
    assert(labs(measurement.temperatureCentiC - 2500) <= 4);
    assert(registers[0] == 0x11U);
    injectedFault = 0x80U;
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_RTD_FAULT);
    assert(measurement.fault == 0x80U && registers[0] == 0x11U);
    injectedFault = 0U;
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_OK);
    injectedCycleFault = 0x10U;
    assert(MAX31865_RunFaultDetection(&cycleFault) == MAX31865_OK);
    assert(cycleFault == 0x10U && registers[0] == 0x11U);
    injectedCycleFault = 0U;
    assert(MAX31865_RunFaultDetection(&cycleFault) == MAX31865_OK);
    assert(cycleFault == 0U && registers[0] == 0x11U);
    faultCycleStuck = true;
    assert(MAX31865_RunFaultDetection(&cycleFault) == MAX31865_CONFIG_ERROR);
    assert(MAX31865_GetConfigDiagnostic().actual == 0x9DU && registers[0] == 0x11U);
    faultCycleStuck = false;
    spiFailure = true;
    assert(MAX31865_RunFaultDetection(&cycleFault) == MAX31865_SPI_ERROR);
    spiFailure = false;
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_OK);
    conversionStuck = true;
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_NOT_READY);
    assert(registers[0] == 0x11U);
    conversionStuck = false;
    failRtdRead = 1U;
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_SPI_ERROR);
    assert(registers[0] == 0x11U);
    stuckMiso = 0;
    assert(MAX31865_Init(430.0f) == MAX31865_CONFIG_ERROR);
    stuckMiso = 255;
    assert(MAX31865_Init(430.0f) == MAX31865_CONFIG_ERROR);
    stuckMiso = -1;
    spiFailure = true;
    assert(MAX31865_Init(430.0f) == MAX31865_SPI_ERROR);
    spiFailure = false;
    assert(MAX31865_Init(430.0f) == MAX31865_OK);
    assert(MAX31865_ReadTemperature(&measurement) == MAX31865_OK);
    puts("PASS: PT100 reference points, ranges, faults, SPI errors and recovery");
    return 0;
}
