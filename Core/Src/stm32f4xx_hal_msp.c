/* stm32f4xx_hal_msp.c — HAL MCU-Specific Package callbacks.
 * Configures GPIO, clocks, and DMA linkage for each peripheral.
 */

#include "main.h"

extern DMA_HandleTypeDef hdma_usart2_rx;

/* ======================================================================
 * HAL_MspInit — called once by HAL_Init()
 * ==================================================================== */
void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();

    /* NVIC priority grouping — 4 bits preempt, 0 bits sub (set in main) */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

/* ======================================================================
 * CAN1 MSP Init
 *   PA11 → CAN1_RX (AF9), PA12 → CAN1_TX (AF9)
 * ==================================================================== */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
    GPIO_InitTypeDef gpio = {0};

    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA11 = CAN1_RX,  PA12 = CAN1_TX */
        gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF9_CAN1;
        HAL_GPIO_Init(GPIOA, &gpio);

        /* CAN RX FIFO0 interrupt (message pending) */
        HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);

        /* CAN status-change / error interrupt */
        HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
    }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_FORCE_RESET();
        __HAL_RCC_CAN1_RELEASE_RESET();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
        HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
        HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
    }
}

/* ======================================================================
 * USART2 MSP Init
 *   PA2 → USART2_TX (AF7), PA3 → USART2_RX (AF7)
 *   DMA1_Stream5 (channel 4) for USART2_RX (circular, byte)
 * ==================================================================== */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance == USART2)
    {
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        /* PA2 = USART2_TX,  PA3 = USART2_RX */
        gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &gpio);

        /* DMA1_Stream5 configured in MX_DMA_Init(); just link it here */
        __HAL_LINKDMA(huart, hdmarx, hdma_usart2_rx);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        __HAL_RCC_USART2_FORCE_RESET();
        __HAL_RCC_USART2_RELEASE_RESET();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
        HAL_DMA_DeInit(huart->hdmarx);
    }
}

/* ======================================================================
 * TIM5 MSP Init (used as run-time stats counter)
 * ==================================================================== */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
        __HAL_RCC_TIM5_CLK_ENABLE();
    }
    else if (htim->Instance == TIM6)
    {
        /* TIM6 is the HAL timebase; only the clock is enabled here.
         * NVIC priority and interrupt enable are set in HAL_InitTick(). */
        __HAL_RCC_TIM6_CLK_ENABLE();
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM5)
    {
        __HAL_RCC_TIM5_FORCE_RESET();
        __HAL_RCC_TIM5_RELEASE_RESET();
    }
    else if (htim->Instance == TIM6)
    {
        __HAL_RCC_TIM6_FORCE_RESET();
        __HAL_RCC_TIM6_RELEASE_RESET();
        HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
    }
}
