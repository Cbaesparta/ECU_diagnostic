/* test_uart_bridge.c — Unit tests for Core/Src/uart_bridge.c
 *
 * UART_Bridge_SendDiagnostics() formats all live CAN and RTOS data as a
 * JSON line and transmits it.  These tests verify:
 *   - NULL-handle guard (no transmit when huart is NULL)
 *   - JSON structure: starts with '{', ends with "}\r\n"
 *   - All vehicle-data fields present with correct values
 *   - CAN health fields (ok, tec, rec, fps, timeout, boff, erp, lec)
 *   - RTOS fields (heap, mheap, ntasks, wm array)
 *   - "ok" field logic: 1 only when frames received AND no bus timeout
 *   - Output fits within UART_TX_BUF_SIZE
 */

#include "test_framework.h"
#include "stub_helpers.h"

#include "can_diagnostic.h"
#include "rtos_monitor.h"
#include "uart_bridge.h"

#include <string.h>
#include <stdio.h>

/* ---- Helpers --------------------------------------------------------- */

static UART_HandleTypeDef fake_uart; /* non-NULL handle for bridge */

static void inject_msg(uint32_t id, uint8_t dlc, const uint8_t data[])
{
    CAN_Msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.id  = id;
    msg.dlc = dlc;
    memcpy(msg.data, data, dlc <= 8u ? dlc : 8u);
    stub_queue_inject(&msg);
}

/* Fully initialise all modules and set up known CAN + RTOS data */
static void setup_with_data(void)
{
    stub_set_tick(0);
    g_stub_CAN1.ESR = 0;

    /* Init CAN module and feed specific frames */
    CAN_Diagnostic_Init(NULL);

    const uint8_t rpm[]   = { 0x0B, 0xB8 };  /* 3000 RPM  */
    const uint8_t spd[]   = { 80 };           /* 80 km/h   */
    const uint8_t tmp[]   = { 125 };          /* 85 °C     */
    const uint8_t bat[]   = { 0x30, 0xD4 };  /* 12500 mV  */
    const uint8_t tps[]   = { 45 };           /* 45 %      */

    inject_msg(CAN_ID_RPM,         2, rpm);
    inject_msg(CAN_ID_SPEED,       1, spd);
    inject_msg(CAN_ID_ENGINE_TEMP, 1, tmp);
    inject_msg(CAN_ID_BATTERY,     2, bat);
    inject_msg(CAN_ID_THROTTLE,    1, tps);

    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();
    CAN_Diagnostic_Process();

    /* Init RTOS monitor with known values */
    stub_set_num_tasks(4);
    stub_set_free_heap(71000);
    stub_set_min_heap(69000);
    stub_set_hwm_value(150);

    /* Set non-NULL task handles so watermarks are reported */
    stub_set_task_handle_can_rx((TaskHandle_t)1);
    stub_set_task_handle_can_diag((TaskHandle_t)2);
    stub_set_task_handle_uart_tx((TaskHandle_t)3);
    stub_set_task_handle_rtos_mon((TaskHandle_t)4);

    RTOS_Monitor_Init();
    stub_set_tick(1234);
    RTOS_Monitor_Update();

    /* Init UART bridge */
    stub_reset_uart_tx();
    UART_Bridge_Init(&fake_uart);
}

/* ======================================================================
 * Guard: NULL huart → no output
 * ==================================================================== */

static void test_null_huart_produces_no_output(void)
{
    stub_reset_uart_tx();
    UART_Bridge_Init(NULL);
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_EQ(0, stub_get_uart_tx_len());
}

/* ======================================================================
 * JSON structure
 * ==================================================================== */

static void test_json_starts_with_brace(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT(stub_get_uart_tx_len() > 0);
    TEST_ASSERT_EQ('{', stub_get_uart_tx_buf()[0]);
}

static void test_json_ends_with_crlf(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    int len = stub_get_uart_tx_len();
    TEST_ASSERT(len >= 2);
    TEST_ASSERT_EQ('\r', stub_get_uart_tx_buf()[len - 2]);
    TEST_ASSERT_EQ('\n', stub_get_uart_tx_buf()[len - 1]);
}

static void test_json_contains_closing_brace_before_crlf(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    int len = stub_get_uart_tx_len();
    TEST_ASSERT(len >= 3);
    TEST_ASSERT_EQ('}', stub_get_uart_tx_buf()[len - 3]);
}

static void test_json_fits_in_tx_buffer(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT(stub_get_uart_tx_len() < (int)UART_TX_BUF_SIZE);
}

/* ======================================================================
 * Vehicle data fields
 * ==================================================================== */

static void test_json_rpm_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"rpm\":3000");
}

static void test_json_speed_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"spd\":80");
}

static void test_json_temp_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"tmp\":85");
}

static void test_json_battery_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"bat\":12500");
}

static void test_json_throttle_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"tps\":45");
}

/* ======================================================================
 * CAN health sub-object
 * ==================================================================== */

static void test_json_can_object_present(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"can\":{");
}

static void test_json_can_ok_is_1_when_frames_received_and_no_timeout(void)
{
    /* frames > 0 and bus_timeout == false → ok=1 */
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"ok\":1");
}

static void test_json_can_ok_is_0_when_no_frames(void)
{
    /* Fresh init, no frames processed → rx_frame_count == 0 → ok=0 */
    stub_reset_uart_tx();
    CAN_Diagnostic_Init(NULL);
    RTOS_Monitor_Init();
    stub_set_tick(0);
    RTOS_Monitor_Update();
    UART_Bridge_Init(&fake_uart);
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"ok\":0");
}

static void test_json_can_ok_is_0_on_bus_timeout(void)
{
    /* Frames received but bus_timeout set → ok=0 */
    setup_with_data();
    /* Trigger timeout via UpdateHealth */
    stub_set_tick(CAN_FRAME_TIMEOUT_MS + 100);
    CAN_Diagnostic_UpdateHealth();
    TEST_ASSERT_EQ(1, (int)CAN_Diagnostic_GetData()->bus_timeout);
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"ok\":0");
}

static void test_json_can_timeout_field_when_timed_out(void)
{
    setup_with_data();
    stub_set_tick(CAN_FRAME_TIMEOUT_MS + 100);
    CAN_Diagnostic_UpdateHealth();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"to\":1");
}

static void test_json_can_boff_field(void)
{
    setup_with_data();
    g_stub_CAN1.ESR = CAN_ESR_BOFF;
    CAN_Diagnostic_UpdateHealth();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"boff\":1");
}

static void test_json_can_erp_field(void)
{
    setup_with_data();
    g_stub_CAN1.ESR = CAN_ESR_EPVF;
    CAN_Diagnostic_UpdateHealth();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"erp\":1");
}

static void test_json_can_lec_field(void)
{
    setup_with_data();
    g_stub_CAN1.ESR = (0x3U << 4);  /* LEC = 3 */
    CAN_Diagnostic_UpdateHealth();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"lec\":3");
}

/* ======================================================================
 * RTOS sub-object
 * ==================================================================== */

static void test_json_rtos_object_present(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"rtos\":{");
}

static void test_json_rtos_heap_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"heap\":71000");
}

static void test_json_rtos_min_heap_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"mheap\":69000");
}

static void test_json_rtos_ntasks_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"ntasks\":4");
}

static void test_json_rtos_watermark_array_present(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"wm\":[");
}

static void test_json_rtos_watermark_contains_task_names(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    const char *buf = stub_get_uart_tx_buf();
    TEST_ASSERT_STR_CONTAINS(buf, "\"CAN_Rx\"");
    TEST_ASSERT_STR_CONTAINS(buf, "\"CAN_Diag\"");
    TEST_ASSERT_STR_CONTAINS(buf, "\"UART_Tx\"");
    TEST_ASSERT_STR_CONTAINS(buf, "\"RTOS_Mon\"");
}

static void test_json_rtos_watermark_empty_when_no_task_handles(void)
{
    /* All task handles are NULL → watermark array should be empty: "wm":[] */
    stub_reset_uart_tx();
    stub_set_task_handle_can_rx(NULL);
    stub_set_task_handle_can_diag(NULL);
    stub_set_task_handle_uart_tx(NULL);
    stub_set_task_handle_rtos_mon(NULL);
    CAN_Diagnostic_Init(NULL);
    RTOS_Monitor_Init();
    stub_set_tick(0);
    RTOS_Monitor_Update();
    UART_Bridge_Init(&fake_uart);
    UART_Bridge_SendDiagnostics();
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"wm\":[]");
}

static void test_json_uptime_field(void)
{
    setup_with_data();
    UART_Bridge_SendDiagnostics();
    /* uptime comes from RTOS monitor (stub tick = 1234 at Update time) */
    TEST_ASSERT_STR_CONTAINS(stub_get_uart_tx_buf(), "\"t\":1234");
}

/* ======================================================================
 * main
 * ==================================================================== */

int main(void)
{
    printf("=== test_uart_bridge ===\n");

    RUN_TEST(test_null_huart_produces_no_output);

    /* JSON structure */
    RUN_TEST(test_json_starts_with_brace);
    RUN_TEST(test_json_ends_with_crlf);
    RUN_TEST(test_json_contains_closing_brace_before_crlf);
    RUN_TEST(test_json_fits_in_tx_buffer);

    /* Vehicle data */
    RUN_TEST(test_json_rpm_field);
    RUN_TEST(test_json_speed_field);
    RUN_TEST(test_json_temp_field);
    RUN_TEST(test_json_battery_field);
    RUN_TEST(test_json_throttle_field);

    /* CAN health */
    RUN_TEST(test_json_can_object_present);
    RUN_TEST(test_json_can_ok_is_1_when_frames_received_and_no_timeout);
    RUN_TEST(test_json_can_ok_is_0_when_no_frames);
    RUN_TEST(test_json_can_ok_is_0_on_bus_timeout);
    RUN_TEST(test_json_can_timeout_field_when_timed_out);
    RUN_TEST(test_json_can_boff_field);
    RUN_TEST(test_json_can_erp_field);
    RUN_TEST(test_json_can_lec_field);

    /* RTOS health */
    RUN_TEST(test_json_rtos_object_present);
    RUN_TEST(test_json_rtos_heap_field);
    RUN_TEST(test_json_rtos_min_heap_field);
    RUN_TEST(test_json_rtos_ntasks_field);
    RUN_TEST(test_json_rtos_watermark_array_present);
    RUN_TEST(test_json_rtos_watermark_contains_task_names);
    RUN_TEST(test_json_rtos_watermark_empty_when_no_task_handles);
    RUN_TEST(test_json_uptime_field);

    TEST_SUITE_RESULTS();
}
