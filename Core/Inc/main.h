#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Peripheral handles (defined in main.c) */
extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim5;
extern DMA_HandleTypeDef  hdma_usart2_rx;

/* Discovery board LED pins (PG13 = LD3 green, PG14 = LD4 red) */
#define LD3_Pin       GPIO_PIN_13
#define LD3_GPIO_Port GPIOG
#define LD4_Pin       GPIO_PIN_14
#define LD4_GPIO_Port GPIOG

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
