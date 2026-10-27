#include "Cpu.h"
#include "uart.h"

void Uart1_Init(void)
{
    lpuart_user_config_t config = lpuart1_InitConfig0;// Initialize the LPUART configuration
    /* Binary 5 kHz stream: receiver must use 921600, 8N1. */
    config.baudRate = 921600U;
    config.transferType = LPUART_USING_INTERRUPTS;// Set the transfer type to use interrupts
    (void)LPUART_DRV_Init(INST_LPUART1, &lpuart1_State, &config);// Initialize the LPUART driver with the specified instance, state, and configuration
}

void Uart1_Send(const uint8_t *data, uint32_t length)
{
    //message 本身是一个数组，它的类型可以理解为const uint8_t[10]
    //'L' 'E' 'D' '5' ' ' 'O' 'N' '\r' '\n' '\0'
    (void)LPUART_DRV_SendDataPolling(INST_LPUART1, data, length);// Send the data using polling mode, which means the function will block until all data is sent
    //为什么发送长度减 1？因为字符串末尾的 '\0' 不需要发送。
    //每次发送前，驱动检查发送数据寄存器是否为空，以及 LPUART 是否空闲，如果空闲就发送数据，否则等待。
    // 'L' -> UART发送寄存器 -> TX引脚
    // 'E' -> UART发送寄存器 -> TX引脚
    // 'D' -> UART发送寄存器 -> TX引脚
    // '5' -> UART发送寄存器 -> TX引脚
    // ' ' -> UART发送寄存器 -> TX引脚
    // 'O' -> UART发送寄存器 -> TX引脚
    // 'N' -> UART发送寄存器 -> TX引脚
    // '\r' -> UART发送寄存器 -> TX引脚
    // '\n' -> UART发送寄存器 -> TX引脚
}
