#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Size of the JSON transmit buffer (bytes).
 * A typical message is ~220 bytes; 512 gives comfortable headroom. */
#define UART_TX_BUF_SIZE 512U

void UART_Bridge_Init(UART_HandleTypeDef *huart);

/* Build a JSON string from current CAN and RTOS data and send it over
 * USART2. Blocks until the transmission completes (HAL_UART_Transmit). */
void UART_Bridge_SendDiagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_BRIDGE_H */
