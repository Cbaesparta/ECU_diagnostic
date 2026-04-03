/* test_framework.h — Minimal test runner for host-based C unit tests.
 *
 * Usage:
 *   static void test_my_thing(void) {
 *       TEST_ASSERT(1 + 1 == 2);
 *       TEST_ASSERT_EQ(42, compute());
 *   }
 *
 *   int main(void) {
 *       printf("=== My tests ===\n");
 *       RUN_TEST(test_my_thing);
 *       TEST_SUITE_RESULTS();
 *   }
 *
 * A test function returns immediately (via the macro) on the first failure.
 * TEST_SUITE_RESULTS() exits main() with 0 on all pass, 1 on any failure.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tf_tests_run    = 0;
static int tf_tests_passed = 0;
static int tf_tests_failed = 0;

/* Assert that condition is true; on failure, print location and return. */
#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("FAIL\n  assertion failed: %s  (at %s:%d)\n", \
                   #cond, __FILE__, __LINE__); \
            tf_tests_failed++; \
            return; \
        } \
    } while (0)

/* Assert two integer/pointer values are equal. */
#define TEST_ASSERT_EQ(expected, actual) \
    do { \
        long long _e = (long long)(expected); \
        long long _a = (long long)(actual); \
        if (_e != _a) { \
            printf("FAIL\n  expected %s == %lld, got %lld  (at %s:%d)\n", \
                   #actual, _e, _a, __FILE__, __LINE__); \
            tf_tests_failed++; \
            return; \
        } \
    } while (0)

/* Assert that needle is a substring of haystack. */
#define TEST_ASSERT_STR_CONTAINS(haystack, needle) \
    do { \
        if (!strstr((haystack), (needle))) { \
            printf("FAIL\n  '%s' not found in: %s  (at %s:%d)\n", \
                   (needle), (haystack), __FILE__, __LINE__); \
            tf_tests_failed++; \
            return; \
        } \
    } while (0)

/* Assert that needle is NOT found in haystack. */
#define TEST_ASSERT_STR_NOT_CONTAINS(haystack, needle) \
    do { \
        if (strstr((haystack), (needle))) { \
            printf("FAIL\n  '%s' unexpectedly found in: %s  (at %s:%d)\n", \
                   (needle), (haystack), __FILE__, __LINE__); \
            tf_tests_failed++; \
            return; \
        } \
    } while (0)

/* Run a single named test function. */
#define RUN_TEST(fn) \
    do { \
        tf_tests_run++; \
        printf("  %-50s ", #fn); \
        fflush(stdout); \
        int _before = tf_tests_failed; \
        fn(); \
        if (tf_tests_failed == _before) { \
            printf("PASS\n"); \
            tf_tests_passed++; \
        } \
    } while (0)

/* Print summary and return exit code from main(). */
#define TEST_SUITE_RESULTS() \
    do { \
        printf("\nResults: %d/%d passed", tf_tests_passed, tf_tests_run); \
        if (tf_tests_failed) { \
            printf(", %d FAILED", tf_tests_failed); \
        } \
        printf("\n"); \
        return (tf_tests_failed == 0) ? 0 : 1; \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
