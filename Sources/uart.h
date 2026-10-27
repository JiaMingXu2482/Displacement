#ifndef UART_H
#define UART_H

#include <stdint.h>

void Uart1_Init(void);
void Uart1_Send(const uint8_t *data, uint32_t length);

#endif /* UART_H */
