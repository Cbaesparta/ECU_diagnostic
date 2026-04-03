/* freertos_stubs.c — Stub implementations for FreeRTOS kernel functions.
 *
 * Queue stub:  a single fixed-size circular buffer (one active queue at a
 *              time, which matches the single CAN RX queue in production).
 * Semaphore:   always succeeds; no actual locking.
 * Task API:    returns controllable test values.
 *
 * Test helpers (stub_*) allow tests to inject messages and configure
 * return values without touching production code.
 */

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/* ======================================================================
 * Task handles — normally defined in freertos.c; provided here so
 * rtos_monitor.c can extern them.
 * ==================================================================== */
TaskHandle_t hCAN_RxTask       = NULL;
TaskHandle_t hCAN_DiagTask     = NULL;
TaskHandle_t hUART_TxTask      = NULL;
TaskHandle_t hRTOS_MonitorTask = NULL;

/* Setters for tests */
void stub_set_task_handle_can_rx(TaskHandle_t h)    { hCAN_RxTask       = h; }
void stub_set_task_handle_can_diag(TaskHandle_t h)  { hCAN_DiagTask     = h; }
void stub_set_task_handle_uart_tx(TaskHandle_t h)   { hUART_TxTask      = h; }
void stub_set_task_handle_rtos_mon(TaskHandle_t h)  { hRTOS_MonitorTask = h; }

/* ======================================================================
 * Queue stub — a circular buffer of fixed-size slots.
 * Item size is captured from xQueueCreate(); all items use that size.
 * ==================================================================== */
#define STUB_QUEUE_SLOTS      32U
#define STUB_QUEUE_SLOT_BYTES 64U

static uint8_t s_q_buf[STUB_QUEUE_SLOTS][STUB_QUEUE_SLOT_BYTES];
static size_t  s_q_item_size = 0;
static int     s_q_head      = 0;
static int     s_q_tail      = 0;
static int     s_q_count     = 0;

/* Inject a message from test code (before calling CAN_Diagnostic_Process) */
void stub_queue_inject(const void *item)
{
    assert(item != NULL);
    assert(s_q_count < (int)STUB_QUEUE_SLOTS);
    assert(s_q_item_size > 0 && s_q_item_size <= STUB_QUEUE_SLOT_BYTES);
    memcpy(s_q_buf[s_q_tail], item, s_q_item_size);
    s_q_tail = (s_q_tail + 1) % (int)STUB_QUEUE_SLOTS;
    s_q_count++;
}

int stub_queue_count(void) { return s_q_count; }

/* ---- FreeRTOS queue API ---------------------------------------------- */

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    (void)uxQueueLength;
    s_q_item_size = (size_t)uxItemSize;
    s_q_head = s_q_tail = s_q_count = 0;
    return (QueueHandle_t)1;  /* non-NULL fake handle */
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                          TickType_t xTicksToWait)
{
    (void)xQueue;
    (void)xTicksToWait;
    if (s_q_count == 0) { return pdFALSE; }
    memcpy(pvBuffer, s_q_buf[s_q_head], s_q_item_size);
    s_q_head = (s_q_head + 1) % (int)STUB_QUEUE_SLOTS;
    s_q_count--;
    return pdTRUE;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue,
                               const void *pvItemToQueue,
                               BaseType_t *pxHigherPriorityTaskWoken)
{
    (void)xQueue;
    if (pxHigherPriorityTaskWoken) { *pxHigherPriorityTaskWoken = pdFALSE; }
    if (s_q_count < (int)STUB_QUEUE_SLOTS) {
        assert(s_q_item_size > 0 && s_q_item_size <= STUB_QUEUE_SLOT_BYTES);
        memcpy(s_q_buf[s_q_tail], pvItemToQueue, s_q_item_size);
        s_q_tail = (s_q_tail + 1) % (int)STUB_QUEUE_SLOTS;
        s_q_count++;
        return pdTRUE;
    }
    return pdFALSE;
}

/* ======================================================================
 * Semaphore / mutex stub — always succeeds; no locking.
 * ==================================================================== */
static int s_mutex_counter = 0;

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    s_mutex_counter++;
    /* Return a small non-NULL integer cast as a pointer */
    return (SemaphoreHandle_t)(uintptr_t)s_mutex_counter;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    (void)xSemaphore;
    (void)xBlockTime;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    (void)xSemaphore;
    return pdTRUE;
}

/* ======================================================================
 * Task API stubs
 * ==================================================================== */
static uint32_t s_num_tasks    = 4;
static uint16_t s_hwm_value    = 100;
static uint32_t s_free_heap    = 70000;
static uint32_t s_min_heap     = 68000;

void stub_set_num_tasks(uint32_t n)    { s_num_tasks = n; }
void stub_set_hwm_value(uint16_t v)    { s_hwm_value = v; }
void stub_set_free_heap(uint32_t v)    { s_free_heap = v; }
void stub_set_min_heap(uint32_t v)     { s_min_heap  = v; }

UBaseType_t uxTaskGetNumberOfTasks(void)
{
    return (UBaseType_t)s_num_tasks;
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask)
{
    (void)xTask;
    return (UBaseType_t)s_hwm_value;
}

uint32_t xPortGetFreeHeapSize(void)             { return s_free_heap; }
uint32_t xPortGetMinimumEverFreeHeapSize(void)  { return s_min_heap;  }

TickType_t xTaskGetTickCount(void) { return (TickType_t)0; }

BaseType_t xTaskCreate(void (*pxTaskCode)(void *),
                        const char *pcName,
                        configSTACK_DEPTH_TYPE usStackDepth,
                        void *pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t *pxCreatedTask)
{
    (void)pxTaskCode; (void)pcName; (void)usStackDepth;
    (void)pvParameters; (void)uxPriority;
    if (pxCreatedTask) { *pxCreatedTask = (TaskHandle_t)1; }
    return pdPASS;
}

void vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement)
{
    (void)pxPreviousWakeTime;
    (void)xTimeIncrement;
}

void vTaskDelay(TickType_t xTicksToDelay) { (void)xTicksToDelay; }
