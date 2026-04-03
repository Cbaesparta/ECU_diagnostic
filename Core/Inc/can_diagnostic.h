#ifndef CAN_DIAGNOSTIC_H
#define CAN_DIAGNOSTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ---- CAN message IDs sent by the Arduino mock ECU -------------------- */
#define CAN_ID_RPM          0x100U   /* Engine RPM      DLC=2 bytes (uint16) */
#define CAN_ID_SPEED        0x101U   /* Vehicle speed   DLC=1 byte  (uint8, km/h) */
#define CAN_ID_ENGINE_TEMP  0x102U   /* Engine temp     DLC=1 byte  (int8, °C offset -40) */
#define CAN_ID_BATTERY      0x103U   /* Battery voltage DLC=2 bytes (uint16, mV) */
#define CAN_ID_THROTTLE     0x104U   /* Throttle pos    DLC=1 byte  (uint8, %) */

/* ---- CAN health thresholds ------------------------------------------- */
#define CAN_TEC_WARNING     96U      /* TEC >= 96  → error-passive territory */
#define CAN_REC_WARNING     96U
#define CAN_FRAME_TIMEOUT_MS 1000U   /* No frame for 1 s → bus silent alert */
#define CAN_RX_QUEUE_DEPTH   32U     /* ISR → task queue depth */

/* ---- CAN frame (used in FreeRTOS queue) ------------------------------- */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} CAN_Msg_t;

/* ---- Live vehicle data and CAN bus health ----------------------------- */
typedef struct {
    /* Vehicle data decoded from CAN frames */
    uint16_t rpm;           /* Engine RPM (0–8000) */
    uint8_t  speed_kmh;     /* Vehicle speed km/h */
    int16_t  engine_temp_c; /* Engine temperature °C */
    uint16_t battery_mv;    /* Battery voltage mV (e.g. 12500 = 12.50 V) */
    uint8_t  throttle_pct;  /* Throttle position % */

    /* CAN bus health */
    uint8_t  tec;               /* Transmit Error Counter (CAN ESR[23:16]) */
    uint8_t  rec;               /* Receive Error Counter  (CAN ESR[31:24]) */
    bool     bus_off;           /* Bus-off state */
    bool     error_passive;     /* Error-passive state */
    uint8_t  last_error_code;   /* LEC field from CAN ESR (0–7) */
    uint32_t rx_frame_count;    /* Total frames received since boot */
    uint32_t rx_error_count;    /* Total HAL-reported CAN errors */
    uint32_t frames_per_sec;    /* Frames/s measured over the last second */
    bool     bus_timeout;       /* True when no frame received for > 1 s */
    uint32_t last_rx_tick;      /* HAL_GetTick() of last received frame */
} CAN_DiagData_t;

/* ---- Public API ------------------------------------------------------ */
void CAN_Diagnostic_Init(CAN_HandleTypeDef *hcan);
void CAN_Diagnostic_Process(void);   /* Call from CAN_RxTask (blocks on queue) */
void CAN_Diagnostic_UpdateHealth(void); /* Call periodically from CAN_DiagTask */
const CAN_DiagData_t *CAN_Diagnostic_GetData(void);
void CAN_Diagnostic_ResetCounters(void);

/* ---- HAL callbacks (defined here, called by HAL) --------------------- */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan);

#ifdef __cplusplus
}
#endif

#endif /* CAN_DIAGNOSTIC_H */
