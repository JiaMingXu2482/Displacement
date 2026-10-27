#ifndef APP_DISPLACEMENT_H
#define APP_DISPLACEMENT_H

#include <stdint.h>
#include "status.h"

/* Adjust to measured values when calibrating the complete circuit. */
#define DISPLACEMENT_VREF_V       3.300f
#define DISPLACEMENT_SHUNT_OHMS   100.0f
#define DISPLACEMENT_RANGE_MM     1500.0f

/* Two-point calibration: ADC code measured at 0 mm and at RANGE_MM.
 * Value below comes from measurements with tools/calibrate_displacement.py:
 *   100 mm -> median 791 (raw 601)
 *   400 mm -> median 3563 (raw 968)
 * Fitted line: 1.223 codes/mm, so 0 mm -> 478 and 1500 mm -> 2313.
 * Ideal 4/20 mA with a true 100 ohm shunt would give 496 and 2482.
 * Re-measure and update these two numbers after any wiring change. */
#define DISPLACEMENT_RAW_ZERO     478
#define DISPLACEMENT_RAW_FULL     2313

typedef struct {
    uint16_t raw;                 /* 12-bit ADC: 0..4095 */
    uint32_t voltageMv;
    uint32_t currentUa;
    int32_t positionTenthMm;      /* 7500 means 750.0 mm; not clamped. */
} displacement_measurement_t;

/* Call after clock and pin initialization. Calibrates ADC0 once. */
status_t Displacement_Init(void);
/* Push completed 64-sample blocks as decimal text lines ("7500\r\n") over UART. */
void Displacement_Process(void);
/* Software-trigger 16 readings and average; includes a conversion timeout. */
status_t Displacement_Read(displacement_measurement_t *measurement);
/* Voltage -> current -> displacement. Pure conversion, no hardware access. */
status_t Displacement_ConvertRaw(uint16_t raw, displacement_measurement_t *measurement);

#endif
