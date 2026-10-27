#include "displacement.h"
#include "adConv1.h"
#include "lpuart1.h"
#include "interrupt_manager.h"
#include "pins_driver.h"
#include "osif.h"
#include <stddef.h>

#define ADC_CONTROL_CHANNEL 0U
#define ADC_SAMPLE_COUNT 16U
#define ADC_TIMEOUT_MS 10U
#define ADC_MAX_VALUE 4095U
#define DISPLACEMENT_STREAM_RATE_HZ 5000U
#define DISPLACEMENT_BLOCK_SAMPLES 64U
#define DISPLACEMENT_RING_SIZE 2048U
/* Widest line is "-32768\r\n" = 8 bytes; one spare byte keeps the bound simple. */
#define DISPLACEMENT_TEXT_MAX 9U
#define DISPLACEMENT_BARRIER() __asm volatile ("dmb" ::: "memory")

static bool initialized;
static volatile uint16_t samples[DISPLACEMENT_RING_SIZE];
static volatile uint32_t produced, consumed;
static volatile uint16_t faultCode;
static uint8_t packet[DISPLACEMENT_BLOCK_SAMPLES * DISPLACEMENT_TEXT_MAX];
static uint32_t lastProgressMs, lastProduced;
static bool transmitting;

/* Append "value\r\n" and return the number of bytes written. */
static uint16_t AppendTenthMm(uint8_t *out, int32_t value)
{
    uint8_t digits[10];
    uint8_t n = 0U;
    uint16_t length = 0U;
    uint32_t magnitude = (uint32_t)(value < 0 ? -value : value);
    do {
        digits[n++] = (uint8_t)('0' + (uint8_t)(magnitude % 10U));
        magnitude /= 10U;
    } while (magnitude != 0U);
    if (value < 0) {
        out[length++] = '-';
    }
    while (n > 0U) {
        out[length++] = digits[--n];
    }
    out[length++] = '\r';
    out[length++] = '\n';
    return length;
}

static void StopWithFault(uint16_t fault)
{
    PDB0->SC = 0U;
    if (faultCode == 0U) faultCode = fault;
}

static void AdcComplete(void)
{
    uint16_t raw;
    ADC_DRV_GetChanResult(INST_ADCONV1, ADC_CONTROL_CHANNEL, &raw);
    if (faultCode != 0U) return;
    if ((uint32_t)(produced - consumed) >= DISPLACEMENT_RING_SIZE) {
        StopWithFault(3U); /* Never silently overwrite samples. */
        return;
    }
    samples[produced & (DISPLACEMENT_RING_SIZE - 1U)] = raw;
    DISPLACEMENT_BARRIER();
    produced++;
}

static void PdbError(void)
{
    if ((PDB0->CH[0].S & PDB_S_ERR_MASK) != 0U) {
        StopWithFault(4U);
        PDB0->CH[0].S = 0U;
    }
}

status_t Displacement_Init(void)
{
    uint32_t clockHz;
    adc_converter_config_t converter = adConv1_ConvConfig0;
    adc_chan_config_t channel = adConv1_ChnConfig0;
    const pin_settings_config_t adcPin = {
        .base = PORTB,
        .pinPortIdx = 3U,
        .pullConfig = PORT_INTERNAL_PULL_NOT_ENABLED,
        .passiveFilter = false,
        .driveSelect = PORT_LOW_DRIVE_STRENGTH,
        .mux = PORT_PIN_DISABLED, /* MUX=0 disables digital function, enables analog. */
        .pinLock = false,
        .intConfig = PORT_DMA_INT_DISABLED,
        .clearIntFlag = false,
        .gpioBase = NULL,
        .digitalFilter = false,
    };

    initialized = false;
    if (CLOCK_SYS_GetFreq(ADC0_CLK, &clockHz) != STATUS_SUCCESS) {
        return STATUS_ERROR;
    }
    clockHz /= 1UL << (uint32_t)adConv1_ConvConfig0.clockDivide;
    if (clockHz < ADC_CLOCK_FREQ_MIN_RUNTIME || clockHz > ADC_CLOCK_FREQ_MAX_RUNTIME ||
        adConv1_ConvConfig0.resolution != ADC_RESOLUTION_12BIT ||
        adConv1_ConvConfig0.trigger != ADC_TRIGGER_SOFTWARE ||
        adConv1_ConvConfig0.continuousConvEnable ||
        adConv1_ChnConfig0.channel != ADC_INPUTCHAN_EXT7 ||
        adConv1_ChnConfig0.interruptEnable) {
        return STATUS_ERROR;
    }

    /* Explicit pin setup also works if pin_mux did not emit the reset-default pin. */
    if (PINS_DRV_Init(1U, &adcPin) != STATUS_SUCCESS) {
        return STATUS_ERROR;
    }
    ADC_DRV_Reset(INST_ADCONV1);
    ADC_DRV_ConfigConverter(INST_ADCONV1, &adConv1_ConvConfig0);
    /* Keep sampling unconditional; averaging below is performed in software. */
    const adc_compare_config_t compare = {false, false, false, 0U, 0U};
    const adc_average_config_t average = {false, ADC_AVERAGE_4};
    ADC_DRV_ConfigHwCompare(INST_ADCONV1, &compare);
    ADC_DRV_ConfigHwAverage(INST_ADCONV1, &average);
    ADC_DRV_AutoCalibration(INST_ADCONV1);
    OSIF_TimeDelay(0U); /* Start OSIF tick used by conversion timeout. */

    produced = consumed = lastProduced = 0U;
    faultCode = 0U;
    transmitting = false;
    lastProgressMs = OSIF_GetMilliseconds();
    PCC->PCCn[PCC_PDB0_INDEX] |= PCC_PCCn_CGC_MASK;
    PDB0->SC = 0U;
    /* PDB uses the system/core clock on S32K144; 48 MHz / 5000 Hz = 9600 ticks. */
    if (CLOCK_SYS_GetFreq(CORE_CLK, &clockHz) != STATUS_SUCCESS ||
        clockHz == 0U || clockHz % DISPLACEMENT_STREAM_RATE_HZ != 0U) {
        return STATUS_ERROR;
    }
    clockHz /= DISPLACEMENT_STREAM_RATE_HZ;
    if (clockHz < 3U || clockHz > 65536U) {
        return STATUS_ERROR;
    }

    converter.trigger = ADC_TRIGGER_HARDWARE;
    converter.pretriggerSel = ADC_PRETRIGGER_SEL_PDB;
    converter.triggerSel = ADC_TRIGGER_SEL_PDB;
    converter.dmaEnable = false;
    converter.continuousConvEnable = false;
    channel.interruptEnable = true;
    ADC_DRV_ConfigConverter(INST_ADCONV1, &converter);
    ADC_DRV_ConfigChan(INST_ADCONV1, ADC_CONTROL_CHANNEL, &channel);
    INT_SYS_InstallHandler(ADC0_IRQn, AdcComplete, (isr_t *)0);
    INT_SYS_SetPriority(ADC0_IRQn, 0U);
    INT_SYS_ClearPending(ADC0_IRQn);
    INT_SYS_EnableIRQ(ADC0_IRQn);
    INT_SYS_InstallHandler(PDB0_IRQn, PdbError, (isr_t *)0);
    INT_SYS_SetPriority(PDB0_IRQn, 0U);
    INT_SYS_ClearPending(PDB0_IRQn);
    INT_SYS_EnableIRQ(PDB0_IRQn);
    INT_SYS_SetPriority(LPUART1_RxTx_IRQn, 2U);

    /* Free-running hardware trigger: 200 us per point, no software latency.
     * PDB0 channel 0 pretrigger 0 -> ADC0 control channel 0 -> PTB3/SE7. */
    PDB0->MOD = clockHz - 1U;
    PDB0->IDLY = clockHz - 1U;
    PDB0->CH[0].C1 = PDB_C1_EN(1U) | PDB_C1_TOS(1U);
    PDB0->CH[0].S = 0U;
    PDB0->CH[0].DLY[0] = 1U;
    PDB0->CH[1].C1 = 0U;
    PDB0->POEN = 0U;
    PDB0->SC = PDB_SC_TRGSEL(15U) | PDB_SC_CONT_MASK | PDB_SC_PDBEIE_MASK |
               PDB_SC_PDBEN_MASK | PDB_SC_LDOK_MASK;
    PDB0->SC |= PDB_SC_SWTRIG_MASK;
    initialized = true;
    return STATUS_SUCCESS;
}

status_t Displacement_ConvertRaw(uint16_t raw, displacement_measurement_t *measurement)
{
    float voltage, currentMa;
    int32_t num, den;
    if (measurement == NULL || raw > ADC_MAX_VALUE) {
        return STATUS_ERROR;
    }
    voltage = (float)raw * DISPLACEMENT_VREF_V / (float)ADC_MAX_VALUE;
    currentMa = voltage * 1000.0f / DISPLACEMENT_SHUNT_OHMS;
    measurement->raw = raw;
    measurement->voltageMv = (uint32_t)(voltage * 1000.0f + 0.5f);
    measurement->currentUa = (uint32_t)(currentMa * 1000.0f + 0.5f);

    /* Two-point calibrated map: ADC code -> 0.1 mm, rounded to nearest.
     * Integer math avoids the 4 mA pedestal amplifying float error. */
    den = (int32_t)DISPLACEMENT_RAW_FULL - (int32_t)DISPLACEMENT_RAW_ZERO;
    if (den == 0) {
        return STATUS_ERROR;
    }
    num = ((int32_t)raw - (int32_t)DISPLACEMENT_RAW_ZERO) *
          (int32_t)(DISPLACEMENT_RANGE_MM * 10.0f);
    measurement->positionTenthMm = (num >= 0) ? (num + den / 2) / den
                                              : (num - den / 2) / den;
    return STATUS_SUCCESS;
}

status_t Displacement_Read(displacement_measurement_t *measurement)
{
    uint32_t sum = 0U, start;
    uint16_t raw;
    uint8_t i;
    if (measurement == NULL) {
        return STATUS_ERROR;
    }
    *measurement = (displacement_measurement_t){0};
    if (!initialized) {
        return STATUS_ERROR;
    }
    for (i = 0U; i < ADC_SAMPLE_COUNT; i++) {
        /* Writing control channel 0 starts a conversion of external input 7. */
        ADC_DRV_ConfigChan(INST_ADCONV1, ADC_CONTROL_CHANNEL, &adConv1_ChnConfig0);
        start = OSIF_GetMilliseconds();
        while (!ADC_DRV_GetConvCompleteFlag(INST_ADCONV1, ADC_CONTROL_CHANNEL)) {
            if ((uint32_t)(OSIF_GetMilliseconds() - start) >= ADC_TIMEOUT_MS) {
                const adc_chan_config_t disabled = {false, ADC_INPUTCHAN_DISABLED};
                ADC_DRV_ConfigChan(INST_ADCONV1, ADC_CONTROL_CHANNEL, &disabled);
                initialized = false;
                return STATUS_TIMEOUT;
            }
        }
        ADC_DRV_GetChanResult(INST_ADCONV1, ADC_CONTROL_CHANNEL, &raw);
        sum += raw;
    }
    raw = (uint16_t)((sum + ADC_SAMPLE_COUNT / 2U) / ADC_SAMPLE_COUNT);
    return Displacement_ConvertRaw(raw, measurement);
}

void Displacement_Process(void)
{
    uint32_t now = OSIF_GetMilliseconds(), first;
    uint16_t i, count, length = 0U;
    displacement_measurement_t measurement;

    if (produced != lastProduced) {
        lastProduced = produced;
        lastProgressMs = now;
    } else if (faultCode == 0U && (uint32_t)(now - lastProgressMs) > 100U) {
        StopWithFault(5U); /* ADC stopped updating. */
    }
    if (transmitting) {
        status_t status = LPUART_DRV_GetTransmitStatus(INST_LPUART1, (uint32_t *)0);
        if (status == STATUS_BUSY) return;
        transmitting = false;
        if (status != STATUS_SUCCESS) StopWithFault(6U);
    }
    if (faultCode != 0U) return;

    first = consumed;
    if ((uint32_t)(produced - first) < DISPLACEMENT_BLOCK_SAMPLES) return;
    DISPLACEMENT_BARRIER();
    count = DISPLACEMENT_BLOCK_SAMPLES;
    for (i = 0U; i < count; i++) {
        Displacement_ConvertRaw(samples[(first + i) & (DISPLACEMENT_RING_SIZE - 1U)], &measurement);
        /* One sample per line: signed 0.1 mm decimal, then CRLF. 7500 = 750.0 mm. */
        length = (uint16_t)(length + AppendTenthMm(&packet[length], measurement.positionTenthMm));
    }
    DISPLACEMENT_BARRIER();
    consumed = first + count;
    /* packet stays unchanged until the interrupt-driven UART finishes sending. */
    if (LPUART_DRV_SendData(INST_LPUART1, packet, (uint32_t)length) == STATUS_SUCCESS) {
        transmitting = true;
    } else {
        StopWithFault(6U);
    }
}

