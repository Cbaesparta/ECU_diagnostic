/* can_diagnostic.c — CAN1 receive, frame decoding, and health monitoring.
 *
 * Architecture:
 *   HAL_CAN_RxFifo0MsgPendingCallback() runs in ISR context.
 *     → copies the raw frame into g_can_rx_queue (FreeRTOS queue, ISR-safe).
 *
 *   CAN_Diagnostic_Process() runs in CAN_RxTask (blocking wait on queue).
 *     → decodes vehicle data, increments counters.
 *
 *   CAN_Diagnostic_UpdateHealth() runs in CAN_DiagTask (periodic, 200 ms).
 *     → reads TEC/REC from CAN ESR register, computes frame rate, checks timeout.
 */

#include "can_diagnostic.h"
#include "main.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

/* ---- Internal state -------------------------------------------------- */
static CAN_DiagData_t    g_data;
static QueueHandle_t     g_can_rx_queue;
static SemaphoreHandle_t g_data_mutex;

/* Frame-rate measurement: count frames in the last 1-second window */
static uint32_t g_fps_count    = 0;
static uint32_t g_fps_window_start = 0;

/* ======================================================================
 * CAN_Diagnostic_Init
 * Creates the ISR→task queue and data mutex, configures filters,
 * then starts the CAN peripheral and activates interrupts.
 * ==================================================================== */
void CAN_Diagnostic_Init(CAN_HandleTypeDef *hcan)
{
    /* Zero-initialise data */
    memset(&g_data, 0, sizeof(g_data));
    g_fps_window_start = HAL_GetTick();

    /* FreeRTOS objects */
    g_can_rx_queue = xQueueCreate(CAN_RX_QUEUE_DEPTH, sizeof(CAN_Msg_t));
    g_data_mutex   = xSemaphoreCreateMutex();

    configASSERT(g_can_rx_queue != NULL);
    configASSERT(g_data_mutex   != NULL);

    /* Accept all standard (11-bit) frames into FIFO0 */
    CAN_FilterTypeDef filter = {0};
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x0000;   /* 0 = don't care (accept all) */
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = CAN_FILTER_ENABLE;
    if (HAL_CAN_ConfigFilter(hcan, &filter) != HAL_OK) { Error_Handler(); }

    /* Start CAN */
    if (HAL_CAN_Start(hcan) != HAL_OK) { Error_Handler(); }

    /* Enable RX FIFO0 message-pending interrupt + error/bus-off notifications */
    uint32_t notify = CAN_IT_RX_FIFO0_MSG_PENDING |
                      CAN_IT_ERROR_WARNING         |
                      CAN_IT_ERROR_PASSIVE         |
                      CAN_IT_BUSOFF                |
                      CAN_IT_LAST_ERROR_CODE       |
                      CAN_IT_ERROR;
    if (HAL_CAN_ActivateNotification(hcan, notify) != HAL_OK) { Error_Handler(); }
}

/* ======================================================================
 * CAN_Diagnostic_Process — call from CAN_RxTask (blocks on queue)
 * Decodes one frame per call.
 * ==================================================================== */
void CAN_Diagnostic_Process(void)
{
    CAN_Msg_t msg;

    /* Block indefinitely until a frame arrives */
    if (xQueueReceive(g_can_rx_queue, &msg, portMAX_DELAY) != pdTRUE) { return; }

    xSemaphoreTake(g_data_mutex, portMAX_DELAY);

    g_data.rx_frame_count++;
    g_data.last_rx_tick = HAL_GetTick();
    g_data.bus_timeout  = false;
    g_fps_count++;

    switch (msg.id)
    {
        case CAN_ID_RPM:
            /* Bytes 0-1: uint16 big-endian */
            g_data.rpm = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            break;

        case CAN_ID_SPEED:
            g_data.speed_kmh = msg.data[0];
            break;

        case CAN_ID_ENGINE_TEMP:
            /* Byte 0: int8 with -40 °C offset → actual °C = value - 40 */
            g_data.engine_temp_c = (int16_t)((int8_t)msg.data[0]) - 40;
            break;

        case CAN_ID_BATTERY:
            /* Bytes 0-1: uint16 big-endian, value in mV */
            g_data.battery_mv = (uint16_t)((msg.data[0] << 8) | msg.data[1]);
            break;

        case CAN_ID_THROTTLE:
            g_data.throttle_pct = msg.data[0];
            break;

        default:
            break;
    }

    xSemaphoreGive(g_data_mutex);
}

/* ======================================================================
 * CAN_Diagnostic_UpdateHealth — call from CAN_DiagTask every 200 ms.
 * Reads hardware error counters and computes frame rate.
 * ==================================================================== */
void CAN_Diagnostic_UpdateHealth(void)
{
    xSemaphoreTake(g_data_mutex, portMAX_DELAY);

    /* Read CAN Error Status Register directly for TEC/REC/LEC */
    uint32_t esr = CAN1->ESR;
    g_data.tec             = (uint8_t)((esr >> 16) & 0xFFU);
    g_data.rec             = (uint8_t)((esr >> 24) & 0xFFU);
    g_data.bus_off         = (bool)((esr & CAN_ESR_BOFF) != 0U);
    g_data.error_passive   = (bool)((esr & CAN_ESR_EPVF) != 0U);
    g_data.last_error_code = (uint8_t)((esr >> 4) & 0x07U);

    /* Frame-rate: measure over 1-second windows */
    uint32_t now = HAL_GetTick();
    uint32_t elapsed = now - g_fps_window_start;
    if (elapsed >= 1000U)
    {
        g_data.frames_per_sec = (g_fps_count * 1000U) / elapsed;
        g_fps_count           = 0;
        g_fps_window_start    = now;
    }

    /* Bus-silence timeout */
    if (g_data.rx_frame_count > 0U)
    {
        g_data.bus_timeout = ((now - g_data.last_rx_tick) > CAN_FRAME_TIMEOUT_MS);
    }

    xSemaphoreGive(g_data_mutex);
}

/* ======================================================================
 * CAN_Diagnostic_GetData — returns a const pointer to the shared struct.
 * Caller must NOT hold the mutex; this is a snapshot copy for the TX task.
 * ==================================================================== */
const CAN_DiagData_t *CAN_Diagnostic_GetData(void)
{
    return &g_data;
}

/* ======================================================================
 * CAN_Diagnostic_GetSnapshot — copies the current data under mutex into *dst.
 * Use this from any task that must read consistent data without holding a
 * long-lived lock.
 * ==================================================================== */
void CAN_Diagnostic_GetSnapshot(CAN_DiagData_t *dst)
{
    xSemaphoreTake(g_data_mutex, portMAX_DELAY);
    *dst = g_data;
    xSemaphoreGive(g_data_mutex);
}

/* ======================================================================
 * CAN_Diagnostic_ResetCounters
 * ==================================================================== */
void CAN_Diagnostic_ResetCounters(void)
{
    xSemaphoreTake(g_data_mutex, portMAX_DELAY);
    g_data.rx_frame_count  = 0;
    g_data.rx_error_count  = 0;
    g_data.frames_per_sec  = 0;
    g_fps_count            = 0;
    g_fps_window_start     = HAL_GetTick();
    xSemaphoreGive(g_data_mutex);
}

/* ======================================================================
 * HAL CAN callbacks — called from CAN1_RX0_IRQHandler / CAN1_SCE_IRQHandler
 * ==================================================================== */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef header;
    CAN_Msg_t msg;
    BaseType_t woken = pdFALSE;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, msg.data) != HAL_OK)
    {
        return;
    }

    msg.id  = header.StdId;   /* Standard 11-bit frame */
    msg.dlc = (uint8_t)header.DLC;

    xQueueSendFromISR(g_can_rx_queue, &msg, &woken);
    portYIELD_FROM_ISR(woken);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    /* Increment error counter — no mutex needed (atomic 32-bit write on Cortex-M4) */
    g_data.rx_error_count++;

    /* If bus-off, HAL AutoBusOff recovery is enabled; toggle red LED */
    HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
}
