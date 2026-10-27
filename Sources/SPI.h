#ifndef APP_SPI_H
#define APP_SPI_H

#include <stdint.h>
#include "status.h"

/* LPSPI1 transport for MAX31865; transfer keeps CS low for all bytes. */
status_t SPI_Init(void);
status_t SPI_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t length);

#endif /* APP_SPI_H */
