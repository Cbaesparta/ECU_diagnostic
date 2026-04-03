/* hal_stubs.c -- Stub implementations for STM32 HAL functions.
 * Provides controllable test doubles; state is inspectable via
 * the stub_* helper functions declared in stub_helpers.h.
 */

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Global peripheral instances ------------------------------------- */
CAN_TypeDef  g_stub_CAN1  = { .ESR = 0 };
GPIO_TypeDef g_stub_GPIOG = { .reserved = 0 };

CAN_HandleTypeDef  hcan1       = { .Instance = &g_stub_CAN1 };
UART_HandleTypeDef huart2      = { .Instance = NULL };
TIM_HandleTypeDef  htim5       = { .Instance = NULL };
DMA_HandleTypeDef  hdma_usart2_rx = { .Instance = NULL };

/* ---- Controllable tick counter ---------------------------------------- */
static uint32_t s_tick = 0;

void stub_set_tick(uint32_t tick) { s_tick = tick; }
uint32_t stub_get_tick(void)      { return s_tick;  }

uint32_t HAL_GetTick(void) { return s_tick; }

/* ---- HAL_CAN_GetRxMessage injectable state ---------------------------- */
static CAN_RxHeaderTypeDef s_rx_header = { .StdId = 0, .DLC = 0 };
static uint8_t             s_rx_data[8] = { 0 };
static HAL_StatusTypeDef   s_get_rx_result = HAL_OK;

void stub_set_rx_message(uint32_t id, uint8_t dlc, const uint8_t data[])
{
    s_rx_header.StdId = id;
    s_rx_header.DLC   = dlc;
    memcpy(s_rx_data, data, dlc <= 8 ? dlc : 8);
    s_get_rx_result   = HAL_OK;
}

void stub_set_rx_message_fail(void) { s_get_rx_result = HAL_ERROR; }

HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan,
                                        uint32_t RxFifo,
                                        CAN_RxHeaderTypeDef *pHeader,
                                        uint8_t aData[])
{
    (void)hcan;
    (void)RxFifo;
    if (s_get_rx_result != HAL_OK) { return HAL_ERROR; }
    *pHeader = s_rx_header;
    memcpy(aData, s_rx_data, 8);
    return HAL_OK;
}

/* ---- HAL_UART_Transmit capture buffer --------------------------------- */
#define STUB_UART_BUF_SIZE 1024
static char    s_uart_tx_buf[STUB_UART_BUF_SIZE];
static int     s_uart_tx_len = 0;

const char *stub_get_uart_tx_buf(void)  { return s_uart_tx_buf; }
int         stub_get_uart_tx_len(void)  { return s_uart_tx_len; }
void        stub_reset_uart_tx(void)    { s_uart_tx_buf[0] = '\0'; s_uart_tx_len = 0; }

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                     uint8_t *pData,
                                     uint16_t Size,
                                     uint32_t Timeout)
{
    (void)huart;
    (void)Timeout;
    if (Size > 0 && Size < (uint16_t)(STUB_UART_BUF_SIZE - 1)) {
        memcpy(s_uart_tx_buf, pData, Size);
        s_uart_tx_buf[Size] = '\0';
        s_uart_tx_len = (int)Size;
    }
    return HAL_OK;
}

/* ---- Remaining HAL stubs (no-ops) ------------------------------------- */
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                        CAN_FilterTypeDef *sFilterConfig)
{
    (void)hcan;
    (void)sFilterConfig;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan,
                                                uint32_t ActiveITs)
{
    (void)hcan;
    (void)ActiveITs;
    return HAL_OK;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    (void)GPIOx;
    (void)GPIO_Pin;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin,
                       GPIO_PinState PinState)
{
    (void)GPIOx;
    (void)GPIO_Pin;
    (void)PinState;
}

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init)
{
    (void)GPIOx;
    (void)GPIO_Init;
}

HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim)
{
    (void)htim;
    return HAL_OK;
}

void Error_Handler(void)
{
    fprintf(stderr, "Error_Handler() called -- test triggered an unexpected error path\n");
    abort();
}
