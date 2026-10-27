#include "SPI.h"
#include "lpspiCom1.h"
#include "pins_driver.h"
#include <stddef.h>

#define MAX31865_CS_MASK (1UL << 9U) /* PTC9, active low */
#define SPI_TIMEOUT_MS 100U

static bool spiInitialized;

static void ChipSelectDelay(void)
{
    /* At up to 112 MHz, 64 NOPs exceed MAX31865's 400 ns CS timing. */
    volatile uint32_t i;
    for (i = 0U; i < 64U; i++) {
        NOP();
    }
}

status_t SPI_Init(void)
{
    status_t result;
    lpspi_master_config_t config = lpspiCom1_MasterConfig0;
    if (spiInitialized) {
        return STATUS_SUCCESS;
    }

    /* pin_mux.c configures PTC9 as a GPIO output, initially high. */
    PINS_DRV_SetPins(PTC, MAX31865_CS_MASK);

    /* Board W25Q64 shares SPI1: keep its separate PTB17 /CS inactive. */
    PINS_DRV_SetPins(PTB, 1UL << 17U);
    PINS_DRV_SetPinDirection(PTB, 17U, GPIO_OUTPUT_DIRECTION);
    PINS_DRV_SetMuxModeSel(PORTB, 17U, PORT_MUX_AS_GPIO);

    result = CLOCK_SYS_GetFreq(LPSPI1_CLK, &config.lpspiSrcClk);
    if (result != STATUS_SUCCESS || config.lpspiSrcClk == 0U) {
        return STATUS_ERROR;
    }
    /* Match the reference MAX31865 transaction: idle-high clock, sample on second edge. */
    config.bitsPerSec = 500000U;
    config.bitcount = 8U;
    config.clkPolarity = LPSPI_SCK_ACTIVE_LOW;
    config.clkPhase = LPSPI_CLOCK_PHASE_2ND_EDGE;
    config.lsbFirst = false;
    config.transferType = LPSPI_USING_INTERRUPTS;
    result = LPSPI_DRV_MasterInit(LPSPICOM1, &lpspiCom1State, &config);
    spiInitialized = (result == STATUS_SUCCESS);
    return result;
}

status_t SPI_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    status_t result;
    if (!spiInitialized || tx == NULL || rx == NULL || length == 0U) {
        return STATUS_ERROR;
    }
    /* One CS assertion covers both register address and all data bytes. */
    PINS_DRV_ClearPins(PTC, MAX31865_CS_MASK);
    ChipSelectDelay();
    result = LPSPI_DRV_MasterTransferBlocking(LPSPICOM1, tx, rx,
                                            length, SPI_TIMEOUT_MS);
    ChipSelectDelay();
    PINS_DRV_SetPins(PTC, MAX31865_CS_MASK);
    ChipSelectDelay();
    return result;
}
