#ifndef APP_MAX31865_H
#define APP_MAX31865_H

#include <stdint.h>

typedef enum {
    MAX31865_OK = 0,
    MAX31865_SPI_ERROR,
    MAX31865_CONFIG_ERROR,
    MAX31865_RTD_FAULT,
    MAX31865_OUT_OF_RANGE,
    MAX31865_NOT_READY,
    MAX31865_INVALID_ARGUMENT
} max31865_status_t;

typedef struct {
    int32_t temperatureCentiC; /* 2534 means 25.34 degrees Celsius. */
    uint16_t rawRtd;           /* 15-bit ADC value, with fault bit removed. */
    uint16_t rtdRegister;      /* Raw MAX31865 RTD register, including fault bit. */
    uint8_t config;             /* Configuration read immediately before RTD data. */
    uint8_t fault;             /* MAX31865 register 0x07, for troubleshooting. */
} max31865_measurement_t;

typedef struct {
    uint8_t actual;
    uint8_t expected;
} max31865_config_diagnostic_t;

/* Read-back snapshot; meaningful after MAX31865_CONFIG_ERROR. No SPI access. */
max31865_config_diagnostic_t MAX31865_GetConfigDiagnostic(void);

/* Call after clocks, pin mux and UART initialization. RREF is in ohms. */
max31865_status_t MAX31865_Init(float referenceOhms);

/* One-shot, three-wire PT100 measurement; takes approximately 80 ms. */
max31865_status_t MAX31865_ReadTemperature(max31865_measurement_t *measurement);

/* Manual fault-detection cycle, about 30 ms. Clears old fault flags first.
 * Call only after saving the failed measurement. Bias is off on return. */
max31865_status_t MAX31865_RunFaultDetection(uint8_t *fault);

/* IEC 60751 PT100 (100 ohms at 0 C, alpha approximately 0.00385). */
max31865_status_t MAX31865_RawToCelsius(uint16_t raw, float referenceOhms,
                                      int32_t *temperatureCentiC);

#endif /* APP_MAX31865_H */
