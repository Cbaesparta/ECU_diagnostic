#ifndef RTOS_MONITOR_H
#define RTOS_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

#define RTOS_MONITOR_MAX_TASKS 8U   /* Maximum tasks to report watermarks for */

typedef struct {
    char     name[configMAX_TASK_NAME_LEN];
    uint16_t stack_hwm_words; /* Stack high-water mark in 4-byte words */
} RTOS_TaskInfo_t;

typedef struct {
    uint32_t       free_heap_bytes;       /* xPortGetFreeHeapSize() */
    uint32_t       min_ever_heap_bytes;   /* xPortGetMinimumEverFreeHeapSize() */
    uint32_t       uptime_ms;            /* HAL_GetTick() */
    uint8_t        num_tasks;            /* uxTaskGetNumberOfTasks() */
    RTOS_TaskInfo_t tasks[RTOS_MONITOR_MAX_TASKS];
} RTOS_Health_t;

void              RTOS_Monitor_Init(void);
void              RTOS_Monitor_Update(void);
const RTOS_Health_t *RTOS_Monitor_GetData(void);
void              RTOS_Monitor_GetSnapshot(RTOS_Health_t *dst); /* Mutex-protected copy */

#ifdef __cplusplus
}
#endif

#endif /* RTOS_MONITOR_H */
