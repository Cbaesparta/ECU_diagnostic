/* test_can_diagnostic.c — Unit tests for Core/Src/can_diagnostic.c
 *
 * Covers:
 *   CAN_Diagnostic_Process  — frame decoding for all 5 PIDs
 *   CAN_Diagnostic_UpdateHealth — ESR parsing, frame-rate, bus timeout
 *   CAN_Diagnostic_ResetCounters
 *   HAL_CAN_RxFifo0MsgPendingCallback — ISR → queue path
 *   HAL_CAN_ErrorCallback             — error counter increment
 */

#include "test_framework.h"
#include "stub_helpers.h"

#include "can_diagnostic.h"

#include <string.h>

/* ---- Helpers --------------------------------------------------------- */

/* Reset all module state and stubs before each test.
 * CAN_Diagnostic_Init resets g_data and g_fps_window_start but NOT the
 * internal fps frame counter (g_fps_count).  ResetCounters clears that too.
 */
static void setup(void)
{
    stub_set_tick(0);
    g_stub_CAN1.ESR = 0;
    CAN_Diagnostic_Init(NULL);
    CAN_Diagnostic_ResetCounters();
}

/* Build a CAN_Msg_t and inject it into the queue */
static void inject_msg(uint32_t id, uint8_t dlc, const uint8_t data[])
{
    CAN_Msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.id  = id;
    msg.dlc = dlc;
    memcpy(msg.data, data, dlc <= 8u ? dlc : 8u);
    stub_queue_inject(&msg);
}

/* ======================================================================
 * CAN_Diagnostic_Process — RPM decoding
 * ==================================================================== */

static void test_rpm_zero(void)
{
    setup();
    const uint8_t d[] = { 0x00, 0x00 };
    inject_msg(CAN_ID_RPM, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->rpm);
}

static void test_rpm_max(void)
{
    /* 8000 = 0x1F40 */
    setup();
    const uint8_t d[] = { 0x1F, 0x40 };
    inject_msg(CAN_ID_RPM, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(8000, CAN_Diagnostic_GetData()->rpm);
}

static void test_rpm_typical(void)
{
    /* 1500 = 0x05DC */
    setup();
    const uint8_t d[] = { 0x05, 0xDC };
    inject_msg(CAN_ID_RPM, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(1500, CAN_Diagnostic_GetData()->rpm);
}

static void test_rpm_big_endian_byte_order(void)
{
    /* Verify byte 0 is MSB: 0x0100 = 256 */
    setup();
    const uint8_t d[] = { 0x01, 0x00 };
    inject_msg(CAN_ID_RPM, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(256, CAN_Diagnostic_GetData()->rpm);
    /* Swapped should give 1: 0x0001 = 1 */
    setup();
    const uint8_t d2[] = { 0x00, 0x01 };
    inject_msg(CAN_ID_RPM, 2, d2);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(1, CAN_Diagnostic_GetData()->rpm);
}

/* ======================================================================
 * CAN_Diagnostic_Process — Speed decoding
 * ==================================================================== */

static void test_speed_zero(void)
{
    setup();
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->speed_kmh);
}

static void test_speed_typical(void)
{
    setup();
    const uint8_t d[] = { 60 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(60, CAN_Diagnostic_GetData()->speed_kmh);
}

static void test_speed_max(void)
{
    setup();
    const uint8_t d[] = { 255 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(255, CAN_Diagnostic_GetData()->speed_kmh);
}

/* ======================================================================
 * CAN_Diagnostic_Process — Engine temperature decoding
 * Encoding: raw byte = actual_°C + 40  →  actual = byte - 40
 * ==================================================================== */

static void test_temp_zero_celsius(void)
{
    /* 0 °C → raw = 40 */
    setup();
    const uint8_t d[] = { 40 };
    inject_msg(CAN_ID_ENGINE_TEMP, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->engine_temp_c);
}

static void test_temp_minus_40(void)
{
    /* -40 °C → raw = 0 */
    setup();
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_ENGINE_TEMP, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(-40, CAN_Diagnostic_GetData()->engine_temp_c);
}

static void test_temp_typical_warm(void)
{
    /* 85 °C → raw = 125 */
    setup();
    const uint8_t d[] = { 125 };
    inject_msg(CAN_ID_ENGINE_TEMP, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(85, CAN_Diagnostic_GetData()->engine_temp_c);
}

static void test_temp_negative_via_int8_cast(void)
{
    /* raw = 0xFF = 255 → (int8_t)255 = -1 → actual = -1 - 40 = -41 */
    setup();
    const uint8_t d[] = { 0xFF };
    inject_msg(CAN_ID_ENGINE_TEMP, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(-41, CAN_Diagnostic_GetData()->engine_temp_c);
}

static void test_temp_ambient_20c(void)
{
    /* 20 °C → raw = 60 */
    setup();
    const uint8_t d[] = { 60 };
    inject_msg(CAN_ID_ENGINE_TEMP, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(20, CAN_Diagnostic_GetData()->engine_temp_c);
}

/* ======================================================================
 * CAN_Diagnostic_Process — Battery decoding
 * ==================================================================== */

static void test_battery_12500mv(void)
{
    /* 12500 mV = 0x30D4 */
    setup();
    const uint8_t d[] = { 0x30, 0xD4 };
    inject_msg(CAN_ID_BATTERY, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(12500, CAN_Diagnostic_GetData()->battery_mv);
}

static void test_battery_zero(void)
{
    setup();
    const uint8_t d[] = { 0x00, 0x00 };
    inject_msg(CAN_ID_BATTERY, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->battery_mv);
}

static void test_battery_big_endian(void)
{
    /* 0x1234 = 4660 mV */
    setup();
    const uint8_t d[] = { 0x12, 0x34 };
    inject_msg(CAN_ID_BATTERY, 2, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0x1234, CAN_Diagnostic_GetData()->battery_mv);
}

/* ======================================================================
 * CAN_Diagnostic_Process — Throttle decoding
 * ==================================================================== */

static void test_throttle_zero(void)
{
    setup();
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_THROTTLE, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->throttle_pct);
}

static void test_throttle_full(void)
{
    setup();
    const uint8_t d[] = { 100 };
    inject_msg(CAN_ID_THROTTLE, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(100, CAN_Diagnostic_GetData()->throttle_pct);
}

static void test_throttle_mid(void)
{
    setup();
    const uint8_t d[] = { 50 };
    inject_msg(CAN_ID_THROTTLE, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(50, CAN_Diagnostic_GetData()->throttle_pct);
}

/* ======================================================================
 * CAN_Diagnostic_Process — unknown / unhandled CAN IDs
 * ==================================================================== */

static void test_unknown_id_leaves_data_unchanged(void)
{
    setup();
    /* First set known RPM */
    const uint8_t rpm[] = { 0x05, 0xDC };
    inject_msg(CAN_ID_RPM, 2, rpm);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(1500, CAN_Diagnostic_GetData()->rpm);

    /* Now inject an unknown ID */
    const uint8_t unk[] = { 0xFF, 0xFF };
    inject_msg(0x200, 2, unk);
    CAN_Diagnostic_Process();
    /* RPM must be unchanged */
    TEST_ASSERT_EQ(1500, CAN_Diagnostic_GetData()->rpm);
}

static void test_unknown_id_increments_frame_count(void)
{
    /* Even unknown IDs count as received frames */
    setup();
    const uint8_t d[] = { 0x00 };
    inject_msg(0x200, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(1, CAN_Diagnostic_GetData()->rx_frame_count);
}

/* ======================================================================
 * CAN_Diagnostic_Process — frame counter and bus_timeout flag
 * ==================================================================== */

static void test_frame_count_increments_per_message(void)
{
    setup();
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_SPEED, 1, d);
    inject_msg(CAN_ID_SPEED, 1, d);
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(3, CAN_Diagnostic_GetData()->rx_frame_count);
}

static void test_bus_timeout_cleared_after_frame(void)
{
    setup();
    const uint8_t d[] = { 30 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, (int)CAN_Diagnostic_GetData()->bus_timeout);
}

static void test_empty_queue_returns_immediately(void)
{
    /* xQueueReceive returns pdFALSE → Process returns without touching data */
    setup();
    /* No message injected */
    CAN_Diagnostic_Process();
    /* Counters must remain at 0 */
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->rx_frame_count);
}

/* ======================================================================
 * CAN_Diagnostic_UpdateHealth — ESR register parsing
 * ESR layout: TEC[23:16], REC[31:24], BOFF[2], EPVF[1], LEC[6:4]
 * ==================================================================== */

static void test_health_tec_extracted(void)
{
    setup();
    /* TEC = 0x7F at bits [23:16] */
    g_stub_CAN1.ESR = (0x7FU << 16);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0x7F, CAN_Diagnostic_GetData()->tec);
}

static void test_health_rec_extracted(void)
{
    setup();
    /* REC = 0xAB at bits [31:24] */
    g_stub_CAN1.ESR = (0xABU << 24);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0xAB, CAN_Diagnostic_GetData()->rec);
}

static void test_health_bus_off_flag(void)
{
    setup();
    g_stub_CAN1.ESR = CAN_ESR_BOFF;
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(1, (int)CAN_Diagnostic_GetData()->bus_off);
}

static void test_health_bus_off_clear_when_not_set(void)
{
    setup();
    g_stub_CAN1.ESR = 0;
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0, (int)CAN_Diagnostic_GetData()->bus_off);
}

static void test_health_error_passive_flag(void)
{
    setup();
    g_stub_CAN1.ESR = CAN_ESR_EPVF;
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(1, (int)CAN_Diagnostic_GetData()->error_passive);
}

static void test_health_lec_extracted(void)
{
    /* LEC is bits [6:4] → value 0x5 → ESR bit pattern: 0x50 */
    setup();
    g_stub_CAN1.ESR = (0x5U << 4);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(5, CAN_Diagnostic_GetData()->last_error_code);
}

static void test_health_all_esr_fields_zero(void)
{
    setup();
    g_stub_CAN1.ESR = 0;
    CAN_Diagnostic_UpdateHealth();
    const CAN_DiagData_t *d = CAN_Diagnostic_GetData();
    TEST_ASSERT_EQ(0, d->tec);
    TEST_ASSERT_EQ(0, d->rec);
    TEST_ASSERT_EQ(0, (int)d->bus_off);
    TEST_ASSERT_EQ(0, (int)d->error_passive);
    TEST_ASSERT_EQ(0, d->last_error_code);
}

/* ======================================================================
 * CAN_Diagnostic_UpdateHealth — frame rate computation
 * frames_per_sec = (count * 1000) / elapsed_ms (once elapsed >= 1000 ms)
 * ==================================================================== */

static void test_frame_rate_computed_after_one_second(void)
{
    setup();
    /* Receive 50 frames at t=0 */
    stub_set_tick(0);
    const uint8_t d[] = { 0 };
    for (int i = 0; i < 50; i++) {
        inject_msg(CAN_ID_SPEED, 1, d);
        CAN_Diagnostic_Process();
    }
    /* Advance time by exactly 1000 ms, then call health update */
    stub_set_tick(1000);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(50, CAN_Diagnostic_GetData()->frames_per_sec);
}

static void test_frame_rate_not_updated_before_one_second(void)
{
    setup();
    const uint8_t d[] = { 0 };
    for (int i = 0; i < 10; i++) {
        inject_msg(CAN_ID_SPEED, 1, d);
        CAN_Diagnostic_Process();
    }
    /* Only 500 ms elapsed — window not closed yet */
    stub_set_tick(500);
    CAN_Diagnostic_UpdateHealth();
    /* fps should remain 0 (not yet computed) */
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->frames_per_sec);
}

static void test_frame_rate_resets_after_window(void)
{
    setup();
    /* First window: 20 frames in 1000 ms */
    const uint8_t d[] = { 0 };
    for (int i = 0; i < 20; i++) {
        inject_msg(CAN_ID_SPEED, 1, d);
        CAN_Diagnostic_Process();
    }
    stub_set_tick(1000);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(20, CAN_Diagnostic_GetData()->frames_per_sec);

    /* Second window: 0 new frames, advance another 1000 ms */
    stub_set_tick(2000);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->frames_per_sec);
}

/* ======================================================================
 * CAN_Diagnostic_UpdateHealth — bus silence timeout
 * ==================================================================== */

static void test_bus_timeout_not_set_with_no_frames(void)
{
    /* No frames received yet (rx_frame_count == 0) → timeout never asserted */
    setup();
    stub_set_tick(5000);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0, (int)CAN_Diagnostic_GetData()->bus_timeout);
}

static void test_bus_timeout_set_after_silence(void)
{
    /* Receive a frame at t=0, then advance past CAN_FRAME_TIMEOUT_MS */
    setup();
    stub_set_tick(0);
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();

    stub_set_tick(CAN_FRAME_TIMEOUT_MS + 1);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(1, (int)CAN_Diagnostic_GetData()->bus_timeout);
}

static void test_bus_timeout_not_set_within_window(void)
{
    setup();
    stub_set_tick(0);
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();

    /* Just before the timeout boundary */
    stub_set_tick(CAN_FRAME_TIMEOUT_MS - 1);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(0, (int)CAN_Diagnostic_GetData()->bus_timeout);
}

static void test_bus_timeout_clears_on_new_frame(void)
{
    setup();
    stub_set_tick(0);
    const uint8_t d[] = { 0 };
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();

    /* Trigger timeout */
    stub_set_tick(CAN_FRAME_TIMEOUT_MS + 100);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(1, (int)CAN_Diagnostic_GetData()->bus_timeout);

    /* Receive new frame — bus_timeout should clear immediately */
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(0, (int)CAN_Diagnostic_GetData()->bus_timeout);
}

/* ======================================================================
 * CAN_Diagnostic_ResetCounters
 * ==================================================================== */

static void test_reset_counters_zeroes_counts(void)
{
    setup();
    const uint8_t d[] = { 50 };
    inject_msg(CAN_ID_SPEED, 1, d);
    inject_msg(CAN_ID_SPEED, 1, d);
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();

    /* Confirm counts are non-zero */
    TEST_ASSERT(CAN_Diagnostic_GetData()->rx_frame_count == 2);

    CAN_Diagnostic_ResetCounters();

    const CAN_DiagData_t *d2 = CAN_Diagnostic_GetData();
    TEST_ASSERT_EQ(0, d2->rx_frame_count);
    TEST_ASSERT_EQ(0, d2->rx_error_count);
    TEST_ASSERT_EQ(0, d2->frames_per_sec);
}

static void test_reset_counters_preserves_vehicle_data(void)
{
    /* Vehicle sensor values (rpm, speed, etc.) must survive a reset */
    setup();
    const uint8_t rpm[] = { 0x05, 0xDC };
    inject_msg(CAN_ID_RPM, 2, rpm);
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(1500, CAN_Diagnostic_GetData()->rpm);

    CAN_Diagnostic_ResetCounters();

    TEST_ASSERT_EQ(1500, CAN_Diagnostic_GetData()->rpm);
}

/* ======================================================================
 * HAL_CAN_RxFifo0MsgPendingCallback — ISR → queue → Process path
 * ==================================================================== */

static void test_isr_callback_enqueues_message(void)
{
    setup();
    const uint8_t data[] = { 0x13, 0x88, 0, 0, 0, 0, 0, 0 }; /* 5000 RPM */
    stub_set_rx_message(CAN_ID_RPM, 2, data);

    /* Trigger the ISR callback with a fake CAN handle */
    HAL_CAN_RxFifo0MsgPendingCallback(NULL);

    /* The message is now in the queue — process it */
    CAN_Diagnostic_Process();
    TEST_ASSERT_EQ(5000, CAN_Diagnostic_GetData()->rpm);
}

static void test_isr_callback_ignores_hal_error(void)
{
    setup();
    stub_set_rx_message_fail();

    /* Even if HAL_CAN_GetRxMessage fails, no crash and no data change */
    HAL_CAN_RxFifo0MsgPendingCallback(NULL);
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->rx_frame_count);
}

/* ======================================================================
 * HAL_CAN_ErrorCallback
 * ==================================================================== */

static void test_error_callback_increments_error_count(void)
{
    setup();
    TEST_ASSERT_EQ(0, CAN_Diagnostic_GetData()->rx_error_count);
    HAL_CAN_ErrorCallback(NULL);
    TEST_ASSERT_EQ(1, CAN_Diagnostic_GetData()->rx_error_count);
    HAL_CAN_ErrorCallback(NULL);
    TEST_ASSERT_EQ(2, CAN_Diagnostic_GetData()->rx_error_count);
}

/* ======================================================================
 * main
 * ==================================================================== */

int main(void)
{
    printf("=== test_can_diagnostic ===\n");

    /* Frame decoding — RPM */
    RUN_TEST(test_rpm_zero);
    RUN_TEST(test_rpm_max);
    RUN_TEST(test_rpm_typical);
    RUN_TEST(test_rpm_big_endian_byte_order);

    /* Frame decoding — Speed */
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_typical);
    RUN_TEST(test_speed_max);

    /* Frame decoding — Engine temp */
    RUN_TEST(test_temp_zero_celsius);
    RUN_TEST(test_temp_minus_40);
    RUN_TEST(test_temp_typical_warm);
    RUN_TEST(test_temp_negative_via_int8_cast);
    RUN_TEST(test_temp_ambient_20c);

    /* Frame decoding — Battery */
    RUN_TEST(test_battery_12500mv);
    RUN_TEST(test_battery_zero);
    RUN_TEST(test_battery_big_endian);

    /* Frame decoding — Throttle */
    RUN_TEST(test_throttle_zero);
    RUN_TEST(test_throttle_full);
    RUN_TEST(test_throttle_mid);

    /* Unknown IDs and frame counter */
    RUN_TEST(test_unknown_id_leaves_data_unchanged);
    RUN_TEST(test_unknown_id_increments_frame_count);
    RUN_TEST(test_frame_count_increments_per_message);
    RUN_TEST(test_bus_timeout_cleared_after_frame);
    RUN_TEST(test_empty_queue_returns_immediately);

    /* UpdateHealth — ESR */
    RUN_TEST(test_health_tec_extracted);
    RUN_TEST(test_health_rec_extracted);
    RUN_TEST(test_health_bus_off_flag);
    RUN_TEST(test_health_bus_off_clear_when_not_set);
    RUN_TEST(test_health_error_passive_flag);
    RUN_TEST(test_health_lec_extracted);
    RUN_TEST(test_health_all_esr_fields_zero);

    /* UpdateHealth — frame rate */
    RUN_TEST(test_frame_rate_computed_after_one_second);
    RUN_TEST(test_frame_rate_not_updated_before_one_second);
    RUN_TEST(test_frame_rate_resets_after_window);

    /* UpdateHealth — bus timeout */
    RUN_TEST(test_bus_timeout_not_set_with_no_frames);
    RUN_TEST(test_bus_timeout_set_after_silence);
    RUN_TEST(test_bus_timeout_not_set_within_window);
    RUN_TEST(test_bus_timeout_clears_on_new_frame);

    /* ResetCounters */
    RUN_TEST(test_reset_counters_zeroes_counts);
    RUN_TEST(test_reset_counters_preserves_vehicle_data);

    /* ISR callback */
    RUN_TEST(test_isr_callback_enqueues_message);
    RUN_TEST(test_isr_callback_ignores_hal_error);

    /* Error callback */
    RUN_TEST(test_error_callback_increments_error_count);

    TEST_SUITE_RESULTS();
}
