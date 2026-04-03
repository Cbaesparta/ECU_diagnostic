/* FreeRTOS.h — Host-build stub for the FreeRTOS kernel header.
 * Defines essential types and macros without any platform-specific code.
 * Intentionally does NOT include FreeRTOSConfig.h (that file pulls in HAL).
 */
#ifndef FREERTOS_H
#define FREERTOS_H

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/* ---- Core types ------------------------------------------------------- */
typedef uint32_t     TickType_t;
typedef long         BaseType_t;
typedef unsigned long UBaseType_t;
typedef void        *TaskHandle_t;
typedef void        *QueueHandle_t;
typedef void        *SemaphoreHandle_t;

/* ---- Boolean constants ------------------------------------------------ */
#define pdTRUE   ((BaseType_t)1)
#define pdFALSE  ((BaseType_t)0)
#define pdPASS   pdTRUE
#define pdFAIL   pdFALSE

/* ---- Blocking constants ----------------------------------------------- */
#define portMAX_DELAY  ((TickType_t)0xFFFFFFFFUL)

/* ---- FreeRTOS config values used by production code ------------------- */
#define configMAX_TASK_NAME_LEN   16
#define configSTACK_DEPTH_TYPE    uint16_t

/* ---- Assertion (use standard assert in test builds) ------------------- */
#define configASSERT(x)  assert(x)

/* ---- ISR yield (no-op in host tests) ---------------------------------- */
#define portYIELD_FROM_ISR(x)  do { (void)(x); } while (0)

/* ---- Tick conversion (1 ms ticks) ------------------------------------- */
#define pdMS_TO_TICKS(ms)  ((TickType_t)(ms))

/* ---- Heap query functions --------------------------------------------- */
uint32_t xPortGetFreeHeapSize(void);
uint32_t xPortGetMinimumEverFreeHeapSize(void);

/* ---- Interrupt disable stub ------------------------------------------- */
#define taskDISABLE_INTERRUPTS()  do {} while (0)

#endif /* FREERTOS_H */
