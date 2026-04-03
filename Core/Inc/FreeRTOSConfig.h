/* FreeRTOS configuration for STM32F429ZITx @ 168 MHz.
 * CMSIS-RTOS v2 wrapper is used (configured in .ioc).
 * Run-time stats use TIM5 as a high-resolution counter.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
extern uint32_t SystemCoreClock;

/* ---- Architecture ----------------------------------------------------- */
#define configCPU_CLOCK_HZ             ( SystemCoreClock )
#define configTICK_RATE_HZ             ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES           ( 56 )
#define configMINIMAL_STACK_SIZE       ( ( uint16_t ) 128 )
#define configTOTAL_HEAP_SIZE          ( ( size_t ) 75000 )
#define configMAX_TASK_NAME_LEN        ( 16 )
#define configUSE_16_BIT_TICKS         0
#define configIDLE_SHOULD_YIELD        1
#define configUSE_TASK_NOTIFICATIONS   1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 3
#define configUSE_MUTEXES              1
#define configUSE_RECURSIVE_MUTEXES    1
#define configUSE_COUNTING_SEMAPHORES  1
#define configQUEUE_REGISTRY_SIZE      8
#define configUSE_QUEUE_SETS           0
#define configUSE_TIME_SLICING         1
#define configUSE_NEWLIB_REENTRANT     1
#define configENABLE_BACKWARD_COMPATIBILITY 0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5
#define configSTACK_DEPTH_TYPE         uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE size_t

/* ---- Memory allocation ------------------------------------------------ */
#define configSUPPORT_STATIC_ALLOCATION  1
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* ---- Hook functions --------------------------------------------------- */
#define configUSE_IDLE_HOOK              0
#define configUSE_TICK_HOOK              0
#define configCHECK_FOR_STACK_OVERFLOW   2
#define configUSE_MALLOC_FAILED_HOOK     1

/* ---- Run-time stats (CPU usage via TIM5) ------------------------------ */
#define configGENERATE_RUN_TIME_STATS        1
#define configUSE_TRACE_FACILITY             1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

/* TIM5: APB1 timer clock = 84 MHz, prescaler = 167 → 500 kHz counter.
 * The macros below must be visible to tasks.c; forward-declare the handle. */
#include "stm32f4xx_hal.h"
extern TIM_HandleTypeDef htim5;
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() \
    do { HAL_TIM_Base_Start(&htim5); } while(0)
#define portGET_RUN_TIME_COUNTER_VALUE() \
    ( __HAL_TIM_GET_COUNTER(&htim5) )

/* ---- Co-routines (unused) -------------------------------------------- */
#define configUSE_CO_ROUTINES            0
#define configMAX_CO_ROUTINE_PRIORITIES  1

/* ---- Software timers ------------------------------------------------- */
#define configUSE_TIMERS             1
#define configTIMER_TASK_PRIORITY    ( 2 )
#define configTIMER_QUEUE_LENGTH     10
#define configTIMER_TASK_STACK_DEPTH ( configMINIMAL_STACK_SIZE * 2 )

/* ---- Interrupt priority (STM32 NVIC PRIORITYGROUP_4) ----------------- */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
#define configKERNEL_INTERRUPT_PRIORITY   ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - 4 ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - 4 ) )

/* ---- API includes ----------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_xResumeFromISR               1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_uxTaskGetStackHighWaterMark  1
#define INCLUDE_xTaskGetIdleTaskHandle       1
#define INCLUDE_eTaskGetState                1
#define INCLUDE_xEventGroupSetBitFromISR     1
#define INCLUDE_xTimerPendFunctionCall       1
#define INCLUDE_xTaskAbortDelay              1
#define INCLUDE_xTaskGetHandle               1
#define INCLUDE_xTaskResumeFromISR           1

/* ---- Assertion -------------------------------------------------------- */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for(;;); }

/* ---- CMSIS-RTOS v2 compatibility ------------------------------------- */
#define configENABLE_FPU                 1
#define configENABLE_MPU                 0

#endif /* FREERTOS_CONFIG_H */
