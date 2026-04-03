/* freertos.c — FreeRTOS task definitions for ECU Diagnostic firmware.
 *
 * Tasks:
 *   CAN_RxTask       (HIGH priority)   — processes frames from ISR queue
 *   CAN_DiagTask     (ABOVE_NORMAL)    — samples CAN error counters every 200 ms
 *   UART_TxTask      (NORMAL priority) — sends JSON to ESP32 every 500 ms
 *   RTOS_MonitorTask (BELOW_NORMAL)    — collects heap / watermark data every 1 s
 */

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "main.h"
#include "can_diagnostic.h"
#include "rtos_monitor.h"
#include "uart_bridge.h"

/* ---- Task handles (extern'd in rtos_monitor.c) ----------------------- */
TaskHandle_t hCAN_RxTask      = NULL;
TaskHandle_t hCAN_DiagTask    = NULL;
TaskHandle_t hUART_TxTask     = NULL;
TaskHandle_t hRTOS_MonitorTask = NULL;

/* ---- Task stack sizes (words = 4 bytes each) ------------------------- */
#define STACK_CAN_RX        256U
#define STACK_CAN_DIAG      256U
#define STACK_UART_TX       512U   /* Extra for snprintf / JSON formatting */
#define STACK_RTOS_MONITOR  256U

/* ======================================================================
 * CAN_RxTask — highest application priority.
 * Blocks on the internal CAN RX queue populated by the ISR callback.
 * Decodes each frame; toggles the green LED on activity.
 * ==================================================================== */
static void vCAN_RxTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t led_toggle_count = 0;

    for (;;)
    {
        /* CAN_Diagnostic_Process blocks indefinitely waiting for a frame */
        CAN_Diagnostic_Process();

        /* Toggle green LED every 50 frames to show CAN activity */
        if (++led_toggle_count >= 50U)
        {
            HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
            led_toggle_count = 0;
        }
    }
}

/* ======================================================================
 * CAN_DiagTask — samples hardware error counters every 200 ms.
 * ==================================================================== */
static void vCAN_DiagTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(200U);

    for (;;)
    {
        CAN_Diagnostic_UpdateHealth();
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ======================================================================
 * UART_TxTask — formats and transmits the JSON diagnostic packet every 500 ms.
 * ==================================================================== */
static void vUART_TxTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500U);

    /* Brief startup delay so all tasks are running before first TX */
    vTaskDelay(pdMS_TO_TICKS(200U));

    for (;;)
    {
        UART_Bridge_SendDiagnostics();
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ======================================================================
 * RTOS_MonitorTask — snapshots heap and stack watermarks every 1 s.
 * ==================================================================== */
static void vRTOS_MonitorTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000U);

    for (;;)
    {
        RTOS_Monitor_Update();
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/* ======================================================================
 * MX_FREERTOS_Init — creates all tasks.
 * Called from main() before osKernelStart().
 * ==================================================================== */
void MX_FREERTOS_Init(void)
{
    /* RTOS monitor must be initialised before any task runs */
    RTOS_Monitor_Init();

    BaseType_t rc;

    rc = xTaskCreate(vCAN_RxTask,
                     "CAN_Rx",
                     STACK_CAN_RX,
                     NULL,
                     osPriorityHigh,           /* 32 */
                     &hCAN_RxTask);
    configASSERT(rc == pdPASS);

    rc = xTaskCreate(vCAN_DiagTask,
                     "CAN_Diag",
                     STACK_CAN_DIAG,
                     NULL,
                     osPriorityAboveNormal,    /* 28 */
                     &hCAN_DiagTask);
    configASSERT(rc == pdPASS);

    rc = xTaskCreate(vUART_TxTask,
                     "UART_Tx",
                     STACK_UART_TX,
                     NULL,
                     osPriorityNormal,         /* 24 */
                     &hUART_TxTask);
    configASSERT(rc == pdPASS);

    rc = xTaskCreate(vRTOS_MonitorTask,
                     "RTOS_Mon",
                     STACK_RTOS_MONITOR,
                     NULL,
                     osPriorityBelowNormal,    /* 20 */
                     &hRTOS_MonitorTask);
    configASSERT(rc == pdPASS);
}
