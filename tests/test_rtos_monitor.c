/* test_rtos_monitor.c -- Unit tests for Core/Src/rtos_monitor.c
 *
 * Covers:
 *   RTOS_Monitor_Init   -- zeroes all fields
 *   RTOS_Monitor_Update -- populates heap sizes, uptime, task count, task info
 *   RTOS_Monitor_GetData -- returns a valid non-NULL pointer
 *
 * Task handle scenarios:
 *   All handles non-NULL → 4 task entries populated
 *   All handles NULL     → 0 task entries (remaining slots zeroed)
 *   Partial handles      → only non-NULL handles produce entries
 */

#include "test_framework.h"
#include "stub_helpers.h"

#include "rtos_monitor.h"

#include <string.h>

/* ---- Helpers --------------------------------------------------------- */

static void setup_all_handles_nonnull(void)
{
    stub_set_task_handle_can_rx((TaskHandle_t)0x10);
    stub_set_task_handle_can_diag((TaskHandle_t)0x20);
    stub_set_task_handle_uart_tx((TaskHandle_t)0x30);
    stub_set_task_handle_rtos_mon((TaskHandle_t)0x40);
}

static void setup_all_handles_null(void)
{
    stub_set_task_handle_can_rx(NULL);
    stub_set_task_handle_can_diag(NULL);
    stub_set_task_handle_uart_tx(NULL);
    stub_set_task_handle_rtos_mon(NULL);
}

/* ======================================================================
 * RTOS_Monitor_Init
 * ==================================================================== */

static void test_init_zeroes_heap_fields(void)
{
    /* Populate data first, then re-init and check it is zeroed */
    stub_set_free_heap(80000);
    stub_set_min_heap(75000);
    stub_set_num_tasks(4);
    stub_set_tick(9999);
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();

    /* Now reinitialise -- GetData should return the updated values from
     * the most recent Update; we verify Init alone zeroed the snapshot */
    RTOS_Monitor_Init();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();
    TEST_ASSERT_EQ(0, h->free_heap_bytes);
    TEST_ASSERT_EQ(0, h->min_ever_heap_bytes);
    TEST_ASSERT_EQ(0, h->uptime_ms);
    TEST_ASSERT_EQ(0, h->num_tasks);
}

static void test_init_zeroes_task_name_slots(void)
{
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();

    RTOS_Monitor_Init();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();
    for (uint8_t i = 0; i < RTOS_MONITOR_MAX_TASKS; i++) {
        TEST_ASSERT_EQ('\0', h->tasks[i].name[0]);
        TEST_ASSERT_EQ(0,    h->tasks[i].stack_hwm_words);
    }
}

/* ======================================================================
 * RTOS_Monitor_GetData
 * ==================================================================== */

static void test_getdata_returns_non_null(void)
{
    RTOS_Monitor_Init();
    TEST_ASSERT(RTOS_Monitor_GetData() != NULL);
}

/* ======================================================================
 * RTOS_Monitor_Update -- heap and uptime
 * ==================================================================== */

static void test_update_captures_free_heap(void)
{
    stub_set_free_heap(72000);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(72000, RTOS_Monitor_GetData()->free_heap_bytes);
}

static void test_update_captures_min_ever_heap(void)
{
    stub_set_min_heap(65000);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(65000, RTOS_Monitor_GetData()->min_ever_heap_bytes);
}

static void test_update_captures_uptime(void)
{
    stub_set_tick(42000);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(42000, RTOS_Monitor_GetData()->uptime_ms);
}

static void test_update_captures_task_count(void)
{
    stub_set_num_tasks(6);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(6, RTOS_Monitor_GetData()->num_tasks);
}

/* ======================================================================
 * RTOS_Monitor_Update -- task info with all handles non-NULL
 * ==================================================================== */

static void test_update_all_four_task_names_populated(void)
{
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    /* Verify name strings match expected values (order is fixed in source) */
    TEST_ASSERT_EQ(0, strcmp("CAN_Rx",   h->tasks[0].name));
    TEST_ASSERT_EQ(0, strcmp("CAN_Diag", h->tasks[1].name));
    TEST_ASSERT_EQ(0, strcmp("UART_Tx",  h->tasks[2].name));
    TEST_ASSERT_EQ(0, strcmp("RTOS_Mon", h->tasks[3].name));
}

static void test_update_all_four_hwm_values_populated(void)
{
    stub_set_hwm_value(200);
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQ(200, h->tasks[i].stack_hwm_words);
    }
}

static void test_update_unused_slots_are_zeroed(void)
{
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    /* Slots 4..7 must be zeroed since we only have 4 handles */
    for (uint8_t i = 4; i < RTOS_MONITOR_MAX_TASKS; i++) {
        TEST_ASSERT_EQ('\0', h->tasks[i].name[0]);
        TEST_ASSERT_EQ(0,    h->tasks[i].stack_hwm_words);
    }
}

/* ======================================================================
 * RTOS_Monitor_Update -- task info with all handles NULL
 * ==================================================================== */

static void test_update_no_task_info_when_all_handles_null(void)
{
    setup_all_handles_null();
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    for (uint8_t i = 0; i < RTOS_MONITOR_MAX_TASKS; i++) {
        TEST_ASSERT_EQ('\0', h->tasks[i].name[0]);
    }
}

/* ======================================================================
 * RTOS_Monitor_Update -- partial handles (only CAN_Rx non-NULL)
 * ==================================================================== */

static void test_update_only_one_task_when_one_handle_set(void)
{
    setup_all_handles_null();
    stub_set_task_handle_can_rx((TaskHandle_t)0x10);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    TEST_ASSERT_EQ(0, strcmp("CAN_Rx", h->tasks[0].name));
    /* Remaining slots must be empty */
    TEST_ASSERT_EQ('\0', h->tasks[1].name[0]);
    TEST_ASSERT_EQ('\0', h->tasks[2].name[0]);
    TEST_ASSERT_EQ('\0', h->tasks[3].name[0]);
}

static void test_update_two_tasks_when_two_handles_set(void)
{
    setup_all_handles_null();
    stub_set_task_handle_can_rx((TaskHandle_t)0x10);
    stub_set_task_handle_can_diag((TaskHandle_t)0x20);
    RTOS_Monitor_Init();
    RTOS_Monitor_Update();
    const RTOS_Health_t *h = RTOS_Monitor_GetData();

    TEST_ASSERT_EQ(0, strcmp("CAN_Rx",   h->tasks[0].name));
    TEST_ASSERT_EQ(0, strcmp("CAN_Diag", h->tasks[1].name));
    TEST_ASSERT_EQ('\0', h->tasks[2].name[0]);
}

/* ======================================================================
 * RTOS_Monitor_Update -- data snapshot is stable across multiple updates
 * ==================================================================== */

static void test_successive_updates_overwrite_with_latest_values(void)
{
    setup_all_handles_nonnull();
    RTOS_Monitor_Init();

    stub_set_free_heap(80000);
    stub_set_tick(1000);
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(80000, RTOS_Monitor_GetData()->free_heap_bytes);
    TEST_ASSERT_EQ(1000,  RTOS_Monitor_GetData()->uptime_ms);

    stub_set_free_heap(79000);
    stub_set_tick(2000);
    RTOS_Monitor_Update();
    TEST_ASSERT_EQ(79000, RTOS_Monitor_GetData()->free_heap_bytes);
    TEST_ASSERT_EQ(2000,  RTOS_Monitor_GetData()->uptime_ms);
}

/* ======================================================================
 * main
 * ==================================================================== */

int main(void)
{
    printf("=== test_rtos_monitor ===\n");

    RUN_TEST(test_init_zeroes_heap_fields);
    RUN_TEST(test_init_zeroes_task_name_slots);

    RUN_TEST(test_getdata_returns_non_null);

    RUN_TEST(test_update_captures_free_heap);
    RUN_TEST(test_update_captures_min_ever_heap);
    RUN_TEST(test_update_captures_uptime);
    RUN_TEST(test_update_captures_task_count);

    RUN_TEST(test_update_all_four_task_names_populated);
    RUN_TEST(test_update_all_four_hwm_values_populated);
    RUN_TEST(test_update_unused_slots_are_zeroed);

    RUN_TEST(test_update_no_task_info_when_all_handles_null);

    RUN_TEST(test_update_only_one_task_when_one_handle_set);
    RUN_TEST(test_update_two_tasks_when_two_handles_set);

    RUN_TEST(test_successive_updates_overwrite_with_latest_values);

    TEST_SUITE_RESULTS();
}
