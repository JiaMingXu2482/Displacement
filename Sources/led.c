#include "Cpu.h"
#include "led.h"

#define LED5_MASK ((uint32_t)1U << 16)

void Led5_Init(void)
{
    // Initialize pins with the configuration defined in g_pin_mux_InitConfigArr
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);
}

void Led5_On(void)
{
    PINS_DRV_ClearPins(PTD, LED5_MASK);
}
