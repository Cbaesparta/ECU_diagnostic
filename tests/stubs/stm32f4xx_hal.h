/* stm32f4xx_hal.h -- Host-build stub replacing the STM32 HAL.
 * Provides only the types, macros, and function declarations used by
 * can_diagnostic.c, uart_bridge.c, and rtos_monitor.c.
 */
#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

/* ---- HAL status -------------------------------------------------------- */
typedef uint32_t HAL_StatusTypeDef;
#define HAL_OK    ((HAL_StatusTypeDef)0x00U)
#define HAL_ERROR ((HAL_StatusTypeDef)0x01U)

/* ---- GPIO -------------------------------------------------------------- */
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct { uint32_t reserved; } GPIO_TypeDef;
typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

#define GPIO_PIN_13          ((uint16_t)0x2000U)
#define GPIO_PIN_14          ((uint16_t)0x4000U)
#define GPIO_MODE_OUTPUT_PP  0U
#define GPIO_NOPULL          0U
#define GPIO_SPEED_FREQ_LOW  0U

extern GPIO_TypeDef g_stub_GPIOG;
#define GPIOG (&g_stub_GPIOG)

/* ---- CAN peripheral register ------------------------------------------ */
typedef struct {
    uint32_t ESR;    /* Error Status Register -- field tested in UpdateHealth */
} CAN_TypeDef;

extern CAN_TypeDef g_stub_CAN1;
#define CAN1 (&g_stub_CAN1)

/* CAN ESR bit masks */
#define CAN_ESR_BOFF  (1U << 2)   /* Bus-off flag */
#define CAN_ESR_EPVF  (1U << 1)   /* Error-passive flag */

/* ---- CAN handle / filter / RX header ---------------------------------- */
typedef struct { CAN_TypeDef *Instance; } CAN_HandleTypeDef;

typedef struct {
    uint32_t FilterBank;
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdHigh;
    uint32_t FilterMaskIdLow;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterActivation;
} CAN_FilterTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t DLC;
} CAN_RxHeaderTypeDef;

/* CAN filter / interrupt macros */
#define CAN_FILTERMODE_IDMASK           0U
#define CAN_FILTERSCALE_32BIT           1U
#define CAN_RX_FIFO0                    0U
#define CAN_FILTER_ENABLE               1U
#define CAN_IT_RX_FIFO0_MSG_PENDING     (1U << 1)
#define CAN_IT_ERROR_WARNING            (1U << 8)
#define CAN_IT_ERROR_PASSIVE            (1U << 9)
#define CAN_IT_BUSOFF                   (1U << 10)
#define CAN_IT_LAST_ERROR_CODE          (1U << 11)
#define CAN_IT_ERROR                    (1U << 15)

/* ---- UART / TIM / DMA handles (opaque for tests) ---------------------- */
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef struct { void *Instance; } TIM_HandleTypeDef;
typedef struct { void *Instance; } DMA_HandleTypeDef;

/* ---- HAL function stubs ----------------------------------------------- */
uint32_t          HAL_GetTick(void);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                        CAN_FilterTypeDef *sFilterConfig);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(CAN_HandleTypeDef *hcan,
                                                uint32_t ActiveITs);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan,
                                        uint32_t RxFifo,
                                        CAN_RxHeaderTypeDef *pHeader,
                                        uint8_t aData[]);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart,
                                     uint8_t *pData,
                                     uint16_t Size,
                                     uint32_t Timeout);
void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin,
                       GPIO_PinState PinState);
void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim);

/* Peripheral handles defined in main.c -- provided by hal_stubs.c */
extern CAN_HandleTypeDef  hcan1;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef  htim5;
extern DMA_HandleTypeDef  hdma_usart2_rx;

void Error_Handler(void);

/* Interrupt stub */
#define __disable_irq() do {} while (0)

#endif /* STM32F4XX_HAL_H */
