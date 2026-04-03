/* task.h -- Host-build stub for FreeRTOS task API. */
#ifndef TASK_H
#define TASK_H

#include "FreeRTOS.h"

UBaseType_t  uxTaskGetNumberOfTasks(void);
UBaseType_t  uxTaskGetStackHighWaterMark(TaskHandle_t xTask);
TickType_t   xTaskGetTickCount(void);
BaseType_t   xTaskCreate(void (*pxTaskCode)(void *),
                          const char *pcName,
                          configSTACK_DEPTH_TYPE usStackDepth,
                          void *pvParameters,
                          UBaseType_t uxPriority,
                          TaskHandle_t *pxCreatedTask);
void vTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                     TickType_t xTimeIncrement);
void vTaskDelay(TickType_t xTicksToDelay);

/* CMSIS-RTOS v2 priority constants used in freertos.c (not tested directly) */
#define osPriorityHigh         32
#define osPriorityAboveNormal  28
#define osPriorityNormal       24
#define osPriorityBelowNormal  20

#endif /* TASK_H */
