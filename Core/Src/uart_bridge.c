/* uart_bridge.c — Serialises CAN and RTOS diagnostics as a JSON line
 * and transmits it to the ESP32 over USART2 @ 115200 8N1.
 *
 * JSON format (one compact line, terminated with \r\n):
 *
 *   {"t":12345,"rpm":1500,"spd":60,"tmp":85,"bat":12500,"tps":45,
 *    "can":{"ok":1,"tec":0,"rec":0,"fps":50,"rx":1000,"err":2,
 *           "to":0,"boff":0,"erp":0,"lec":0},
 *    "rtos":{"heap":70000,"mheap":68000,"ntasks":4,
 *            "wm":[["CAN_Rx",200],["CAN_Diag",150],
 *                  ["UART_Tx",300],["RTOS_Mon",120]]}}
 *
 * Total length is typically ~240 bytes — well within the 512-byte buffer.
 */

#include "uart_bridge.h"
#include "can_diagnostic.h"
#include "rtos_monitor.h"

#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *g_huart = NULL;
static char g_tx_buf[UART_TX_BUF_SIZE];

/* ======================================================================
 * UART_Bridge_Init
 * ==================================================================== */
void UART_Bridge_Init(UART_HandleTypeDef *huart)
{
    g_huart = huart;
}

/* ======================================================================
 * UART_Bridge_SendDiagnostics
 * Builds the JSON string and transmits it.  Blocks until TX is complete.
 * ==================================================================== */
void UART_Bridge_SendDiagnostics(void)
{
    if (g_huart == NULL) { return; }

    /* Take mutex-protected snapshots so we read consistent data even if
     * CAN_DiagTask or RTOS_MonitorTask is updating concurrently. */
    CAN_DiagData_t can_snap;
    RTOS_Health_t  rtos_snap;
    CAN_Diagnostic_GetSnapshot(&can_snap);
    RTOS_Monitor_GetSnapshot(&rtos_snap);

    const CAN_DiagData_t  *c = &can_snap;
    const RTOS_Health_t   *r = &rtos_snap;

    /* ---- Watermark array -------------------------------------------- */
    /* Pre-format the watermark JSON array so snprintf stays readable */
    char wm_buf[192] = {0};
    int  wm_len = 0;
    for (uint8_t i = 0; i < RTOS_MONITOR_MAX_TASKS; i++)
    {
        if (r->tasks[i].name[0] == '\0') { break; }
        /* Guard against a previous snprintf truncation/error */
        if (wm_len < 0 || (size_t)wm_len >= sizeof(wm_buf)) { break; }
        int written = snprintf(wm_buf + wm_len,
                               sizeof(wm_buf) - (size_t)wm_len,
                               "%s[\"%s\",%u]",
                               (i == 0) ? "" : ",",
                               r->tasks[i].name,
                               r->tasks[i].stack_hwm_words);
        if (written > 0) { wm_len += written; }
    }

    /* ---- Main JSON body --------------------------------------------- */
    int len = snprintf(g_tx_buf, sizeof(g_tx_buf),
        "{"
            "\"t\":%lu,"
            "\"rpm\":%u,"
            "\"spd\":%u,"
            "\"tmp\":%d,"
            "\"bat\":%u,"
            "\"tps\":%u,"
            "\"can\":{"
                "\"ok\":%u,"
                "\"tec\":%u,"
                "\"rec\":%u,"
                "\"fps\":%lu,"
                "\"rx\":%lu,"
                "\"err\":%lu,"
                "\"to\":%u,"
                "\"boff\":%u,"
                "\"erp\":%u,"
                "\"lec\":%u"
            "},"
            "\"rtos\":{"
                "\"heap\":%lu,"
                "\"mheap\":%lu,"
                "\"ntasks\":%u,"
                "\"wm\":[%s]"
            "}"
        "}\r\n",
        /* Vehicle data */
        (unsigned long)r->uptime_ms,
        c->rpm,
        c->speed_kmh,
        (int)c->engine_temp_c,
        c->battery_mv,
        c->throttle_pct,
        /* CAN health */
        (unsigned)(c->rx_frame_count > 0U && !c->bus_timeout ? 1U : 0U),
        c->tec,
        c->rec,
        (unsigned long)c->frames_per_sec,
        (unsigned long)c->rx_frame_count,
        (unsigned long)c->rx_error_count,
        (unsigned)c->bus_timeout,
        (unsigned)c->bus_off,
        (unsigned)c->error_passive,
        c->last_error_code,
        /* RTOS health */
        (unsigned long)r->free_heap_bytes,
        (unsigned long)r->min_ever_heap_bytes,
        r->num_tasks,
        wm_buf
    );

    if (len <= 0 || len >= (int)sizeof(g_tx_buf)) { return; }

    /* Blocking TX; 100 ms timeout is sufficient for ~240 bytes @ 115200 baud.
     * Capture the return value: a non-OK result (HAL_BUSY, HAL_TIMEOUT, or
     * HAL_ERROR) means this cycle's packet was lost.  The next 500 ms cycle
     * will retry automatically — no special recovery is needed here. */
    HAL_StatusTypeDef tx_status =
        HAL_UART_Transmit(g_huart, (uint8_t *)g_tx_buf, (uint16_t)len, 100U);
    (void)tx_status;
}
