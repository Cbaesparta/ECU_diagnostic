/* main.c — ECU Diagnostic Firmware for STM32F429ZITx
 *
 * Hardware:
 *   CAN1  (PA11/PA12) @ 1 Mbps  → TJA1050 → CAN bus → Arduino mock ECU
 *   USART2 (PA2/PA3) @ 115200 8N1 → ESP32 (web dashboard bridge)
 *   TIM5  — high-resolution run-time stats counter (500 kHz, prescaler=167)
 *   TIM6  — HAL SysTick base (NVIC priority 15)
 *
 * FreeRTOS tasks (defined in freertos.c):
 *   CAN_RxTask      — processes received CAN frames from ISR queue
 *   CAN_DiagTask    — samples CAN error counters & frame rate every 200 ms
 *   UART_TxTask     — serialises diagnostics to ESP32 every 500 ms
 *   RTOS_MonitorTask— collects RTOS health metrics every 1 s
 */

#include "main.h"
#include "can_diagnostic.h"
#include "rtos_monitor.h"
#include "uart_bridge.h"
#include "cmsis_os.h"

/* ---- Peripheral handles (extern'd in main.h) ------------------------- */
CAN_HandleTypeDef  hcan1;
UART_HandleTypeDef huart2;
TIM_HandleTypeDef  htim5;
TIM_HandleTypeDef  htim6;   /* HAL timebase — TIM6 replaces SysTick (used by FreeRTOS) */
DMA_HandleTypeDef  hdma_usart2_rx;

/* ---- Forward declarations -------------------------------------------- */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CAN1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM5_Init(void);

/* FreeRTOS task creation lives in freertos.c */
extern void MX_FREERTOS_Init(void);

/* ======================================================================
 * main
 * ==================================================================== */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_CAN1_Init();
    MX_USART2_UART_Init();
    MX_TIM5_Init();

    /* Initialise application modules */
    CAN_Diagnostic_Init(&hcan1);
    UART_Bridge_Init(&huart2);

    /* Create FreeRTOS tasks and start scheduler */
    MX_FREERTOS_Init();
    osKernelStart();

    /* Should never reach here */
    for (;;) {}
}

/* ======================================================================
 * System clock: HSE 8 MHz → PLL → 168 MHz (APB1=42 MHz, APB2=84 MHz)
 * ==================================================================== */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 168;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { Error_Handler(); }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;   /* 42 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;   /* 84 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}

/* ======================================================================
 * CAN1: PA11=RX, PA12=TX — 1 Mbps (Prescaler=3, BS1=11TQ, BS2=2TQ)
 * ==================================================================== */
static void MX_CAN1_Init(void)
{
    hcan1.Instance                  = CAN1;
    hcan1.Init.Prescaler            = 3;
    hcan1.Init.Mode                 = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth        = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1             = CAN_BS1_11TQ;
    hcan1.Init.TimeSeg2             = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode    = DISABLE;
    hcan1.Init.AutoBusOff           = ENABLE;   /* Auto-recover from bus-off */
    hcan1.Init.AutoWakeUp           = DISABLE;
    hcan1.Init.AutoRetransmission   = ENABLE;
    hcan1.Init.ReceiveFifoLocked    = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK) { Error_Handler(); }
}

/* ======================================================================
 * USART2: PA2=TX, PA3=RX — 115200 8N1, DMA circular on RX
 * ==================================================================== */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) { Error_Handler(); }
}

/* ======================================================================
 * TIM5: 32-bit free-running counter at 500 kHz (for FreeRTOS run-time
 * stats). APB1 timer clock = 84 MHz; prescaler 167 → 84/168 = 500 kHz.
 * ==================================================================== */
static void MX_TIM5_Init(void)
{
    htim5.Instance               = TIM5;
    htim5.Init.Prescaler         = 167;
    htim5.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim5.Init.Period            = 0xFFFFFFFFU;
    htim5.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim5) != HAL_OK) { Error_Handler(); }
    /* Timer is started by portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() */
}

/* ======================================================================
 * DMA: USART2_RX → DMA1 Stream5, circular byte mode
 * Must be initialised before UART.
 * ==================================================================== */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_usart2_rx.Instance                 = DMA1_Stream5;
    hdma_usart2_rx.Init.Channel             = DMA_CHANNEL_4;
    hdma_usart2_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode                = DMA_CIRCULAR;
    hdma_usart2_rx.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_usart2_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK) { Error_Handler(); }
    /* DMA↔UART linkage is performed in HAL_UART_MspInit (called by HAL_UART_Init) */

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/* ======================================================================
 * GPIO: LEDs on PG13 (green) and PG14 (red) for status indication
 * ==================================================================== */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin   = LD3_Pin | LD4_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &gpio);

    /* LEDs off at startup */
    HAL_GPIO_WritePin(GPIOG, LD3_Pin | LD4_Pin, GPIO_PIN_RESET);
}

/* ======================================================================
 * HAL timebase: TIM6 at 1 ms.
 *
 * FreeRTOS owns SysTick, so the HAL tick is driven by TIM6 instead.
 * These three functions override the __weak defaults in the STM32 HAL.
 * HAL_InitTick is called by HAL_Init() and again by HAL_RCC_ClockConfig()
 * after the PLL is configured, so the prescaler always matches the actual
 * APB1 frequency at the time of the call.
 * ==================================================================== */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    RCC_ClkInitTypeDef clk     = {0};
    uint32_t           pFLatency;
    uint32_t           uwTimclock;
    uint32_t           uwPrescalerValue;

    /* Enable TIM6 clock */
    __HAL_RCC_TIM6_CLK_ENABLE();

    /* TIM6 is on APB1; its timer clock is 2 × APB1 */
    HAL_RCC_GetClockConfig(&clk, &pFLatency);
    uwTimclock = 2U * HAL_RCC_GetPCLK1Freq();

    /* Prescaler divides down to 1 MHz so Period = 999 gives exactly 1 ms */
    uwPrescalerValue = (uwTimclock / 1000000U) - 1U;

    htim6.Instance               = TIM6;
    htim6.Init.Period            = (1000000U / 1000U) - 1U;  /* 999 → 1 ms */
    htim6.Init.Prescaler         = uwPrescalerValue;
    htim6.Init.ClockDivision     = 0U;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) { return HAL_ERROR; }

    /* Set and enable the TIM6 update interrupt */
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

    return HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}

/* Called by HAL_TIM_IRQHandler (from TIM6_DAC_IRQHandler) every 1 ms */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        HAL_IncTick();
    }
}

/* ======================================================================
 * Error handler — blink red LED rapidly
 * ==================================================================== */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
        HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
        for (volatile uint32_t d = 0; d < 400000UL; d++) {}
    }
}

/* ======================================================================
 * FreeRTOS stack-overflow hook
 * ==================================================================== */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    Error_Handler();
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
