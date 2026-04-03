/* queue.h -- Host-build stub for FreeRTOS queue API. */
#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS.h"

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t    xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                             TickType_t xTicksToWait);
BaseType_t    xQueueSendFromISR(QueueHandle_t xQueue,
                                 const void *pvItemToQueue,
                                 BaseType_t *pxHigherPriorityTaskWoken);

#endif /* QUEUE_H */
