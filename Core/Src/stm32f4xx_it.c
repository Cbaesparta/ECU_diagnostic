/* stm32f4xx_it.c — Interrupt Service Routines */

#include "main.h"
#include "stm32f4xx_it.h"
#include "cmsis_os.h"

/* Peripheral handles */
extern TIM_HandleTypeDef  htim6;       /* HAL tick timer */
extern DMA_HandleTypeDef  hdma_usart2_rx;
extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart2;

/* ---- Cortex-M4 exception handlers ----------------------------------- */

void NMI_Handler(void)
{
    while (1) {}
}

void HardFault_Handler(void)
{
    while (1) {}
}

void MemManage_Handler(void)
{
    while (1) {}
}

void BusFault_Handler(void)
{
    while (1) {}
}

void UsageFault_Handler(void)
{
    while (1) {}
}

void DebugMon_Handler(void)
{
}

/* SVC and PendSV are handled by FreeRTOS port */

/* ---- HAL tick timer (TIM6) ----------------------------------------- */
void TIM6_DAC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim6);
}

/* ---- DMA1 Stream5 (USART2 RX) -------------------------------------- */
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx);
}

/* ---- CAN1 FIFO0 message pending ------------------------------------ */
void CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan1);
}

/* ---- CAN1 Status-change / Error ------------------------------------ */
void CAN1_SCE_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan1);
}
