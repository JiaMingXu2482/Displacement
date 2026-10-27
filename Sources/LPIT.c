#include "Cpu.h"
#include "lpit1.h"
#include "LPIT.h"
#include "uart.h"

#define LPIT_CHANNEL_0_MASK (1U << 0U)

void LPIT_Init(uint32_t periodUs)
{
    lpit_user_channel_config_t channelConfig = lpit1_ChnConfig0;

    /* Keep the period in microseconds, independent of the LPIT clock source. */
    channelConfig.periodUnits = LPIT_PERIOD_UNITS_MICROSECONDS;
    channelConfig.period = periodUs;
    /* Poll the timeout flag in Process(); no interrupt handler is needed. */
    channelConfig.isInterruptEnabled = false;

    LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
    (void)LPIT_DRV_InitChannel(INST_LPIT1, 0U, &channelConfig);
    LPIT_DRV_StartTimerChannels(INST_LPIT1, LPIT_CHANNEL_0_MASK);
}

bool LPIT_HasElapsed(void)
{
    if (LPIT_DRV_GetInterruptFlagTimerChannels(
            INST_LPIT1, LPIT_CHANNEL_0_MASK) != 0U) {
        /* Acknowledge the timeout; the periodic timer keeps counting. */
        LPIT_DRV_ClearInterruptFlagTimerChannels(
            INST_LPIT1, LPIT_CHANNEL_0_MASK);
        return true;
    }
    return false;
}

void LPIT_Process(const uint8_t *message, uint32_t length)
{
    if (LPIT_HasElapsed()) {
        Uart1_Send(message, length);
    }
}
