/* stub_helpers.h — Declares test-helper functions exposed by the stubs.
 * Include this in test files to access injectable state.
 */
#ifndef STUB_HELPERS_H
#define STUB_HELPERS_H

#include <stdint.h>
#include "FreeRTOS.h"

/* ---- HAL helpers (hal_stubs.c) --------------------------------------- */
void        stub_set_tick(uint32_t tick);
uint32_t    stub_get_tick(void);

void        stub_set_rx_message(uint32_t id, uint8_t dlc, const uint8_t data[]);
void        stub_set_rx_message_fail(void);

const char *stub_get_uart_tx_buf(void);
int         stub_get_uart_tx_len(void);
void        stub_reset_uart_tx(void);

/* ---- FreeRTOS queue helpers (freertos_stubs.c) ----------------------- */
void stub_queue_inject(const void *item);
int  stub_queue_count(void);

/* ---- FreeRTOS task helpers (freertos_stubs.c) ------------------------ */
void stub_set_num_tasks(uint32_t n);
void stub_set_hwm_value(uint16_t v);
void stub_set_free_heap(uint32_t v);
void stub_set_min_heap(uint32_t v);

void stub_set_task_handle_can_rx(TaskHandle_t h);
void stub_set_task_handle_can_diag(TaskHandle_t h);
void stub_set_task_handle_uart_tx(TaskHandle_t h);
void stub_set_task_handle_rtos_mon(TaskHandle_t h);

/* ---- CAN1 ESR register (accessible via macro CAN1->ESR) -------------- */
#include "stm32f4xx_hal.h"
/* g_stub_CAN1.ESR can be set directly: g_stub_CAN1.ESR = value; */

#endif /* STUB_HELPERS_H */
