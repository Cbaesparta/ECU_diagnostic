/* rtos_monitor.c — Collects FreeRTOS health metrics.
 *
 * Called from RTOS_MonitorTask every 1 second.
 * Reads: free heap, minimum ever heap, number of tasks, and the stack
 * high-water mark for each known task handle.
 */

#include "rtos_monitor.h"
#include "stm32f4xx_hal.h"
#include "semphr.h"

/* Task handles are defined in freertos.c and extern'd below */
extern TaskHandle_t hCAN_RxTask;
extern TaskHandle_t hCAN_DiagTask;
extern TaskHandle_t hUART_TxTask;
extern TaskHandle_t hRTOS_MonitorTask;

static RTOS_Health_t  g_health;
static SemaphoreHandle_t g_mutex;

/* ======================================================================
 * Called once before tasks start (from MX_FREERTOS_Init)
 * ==================================================================== */
void RTOS_Monitor_Init(void)
{
    memset(&g_health, 0, sizeof(g_health));
    g_mutex = xSemaphoreCreateMutex();
    configASSERT(g_mutex != NULL);
}

/* ======================================================================
 * RTOS_Monitor_Update — snapshots health metrics.
 * Call periodically from RTOS_MonitorTask.
 * ==================================================================== */
void RTOS_Monitor_Update(void)
{
    RTOS_Health_t snap = {0};

    snap.free_heap_bytes     = xPortGetFreeHeapSize();
    snap.min_ever_heap_bytes = xPortGetMinimumEverFreeHeapSize();
    snap.uptime_ms           = HAL_GetTick();
    snap.num_tasks           = (uint8_t)uxTaskGetNumberOfTasks();

    uint8_t i = 0;

    /* CAN_RxTask */
    if (hCAN_RxTask && i < RTOS_MONITOR_MAX_TASKS)
    {
        strncpy(snap.tasks[i].name, "CAN_Rx", configMAX_TASK_NAME_LEN - 1);
        snap.tasks[i].stack_hwm_words = (uint16_t)uxTaskGetStackHighWaterMark(hCAN_RxTask);
        i++;
    }
    /* CAN_DiagTask */
    if (hCAN_DiagTask && i < RTOS_MONITOR_MAX_TASKS)
    {
        strncpy(snap.tasks[i].name, "CAN_Diag", configMAX_TASK_NAME_LEN - 1);
        snap.tasks[i].stack_hwm_words = (uint16_t)uxTaskGetStackHighWaterMark(hCAN_DiagTask);
        i++;
    }
    /* UART_TxTask */
    if (hUART_TxTask && i < RTOS_MONITOR_MAX_TASKS)
    {
        strncpy(snap.tasks[i].name, "UART_Tx", configMAX_TASK_NAME_LEN - 1);
        snap.tasks[i].stack_hwm_words = (uint16_t)uxTaskGetStackHighWaterMark(hUART_TxTask);
        i++;
    }
    /* RTOS_MonitorTask (self) */
    if (hRTOS_MonitorTask && i < RTOS_MONITOR_MAX_TASKS)
    {
        strncpy(snap.tasks[i].name, "RTOS_Mon", configMAX_TASK_NAME_LEN - 1);
        snap.tasks[i].stack_hwm_words = (uint16_t)uxTaskGetStackHighWaterMark(hRTOS_MonitorTask);
        i++;
    }

    /* Zero unused slots */
    for (; i < RTOS_MONITOR_MAX_TASKS; i++)
    {
        memset(&snap.tasks[i], 0, sizeof(snap.tasks[i]));
    }

    xSemaphoreTake(g_mutex, portMAX_DELAY);
    g_health = snap;
    xSemaphoreGive(g_mutex);
}

/* ======================================================================
 * RTOS_Monitor_GetData — returns a const pointer; caller takes mutex copy.
 * ==================================================================== */
const RTOS_Health_t *RTOS_Monitor_GetData(void)
{
    return &g_health;
}

/* ======================================================================
 * RTOS_Monitor_GetSnapshot — copies the current health data under mutex.
 * Use this from any task that must read a consistent snapshot.
 * ==================================================================== */
void RTOS_Monitor_GetSnapshot(RTOS_Health_t *dst)
{
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    *dst = g_health;
    xSemaphoreGive(g_mutex);
}
