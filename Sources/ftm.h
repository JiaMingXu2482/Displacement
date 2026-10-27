#ifndef FTM_H
#define FTM_H

#include <stdint.h>
#include "Cpu.h"

void Ftm0_Led6PwmInit(void);
status_t Ftm0_Led6PwmSetDuty(uint16_t dutyCycle);
uint16_t Ftm0_Led6PwmGetDuty(void);
void Ftm0_Led6PwmBreathProcess(void);

#endif /* FTM_H */
