/* semphr.h -- Host-build stub for FreeRTOS semaphore/mutex API. */
#ifndef SEMPHR_H
#define SEMPHR_H

#include "FreeRTOS.h"

SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t        xSemaphoreTake(SemaphoreHandle_t xSemaphore,
                                  TickType_t xBlockTime);
BaseType_t        xSemaphoreGive(SemaphoreHandle_t xSemaphore);

#endif /* SEMPHR_H */
