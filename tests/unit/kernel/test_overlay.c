/* test_overlay.c - snapshot / restore of the RAM overlay (no ATA ports) */

#include <string.h>
#include "../../framework/unity.h"
#include "../../../fs/overlay.h"

static uint8_t g_snap[OV_SNAP_SIZE + 64];

void setUp(void) {
    overlay_init();
}

void tearDown(void) {
}

static void test_snapshot_empty_roundtrip(void) {
    uint32_t sz = 0;
    char buf[16];

    setUp();
    TEST_ASSERT_EQUAL(0, overlay_snapshot(g_snap, sizeof(g_snap), &sz));
    TEST_ASSERT_EQUAL(OV_SNAP_SIZE, sz);
    TEST_ASSERT_EQUAL(1, overlay_write("keep.txt", "x", 1));
    TEST_ASSERT_EQUAL(0, overlay_restore(g_snap, sz));
    TEST_ASSERT_TRUE(overlay_read("keep.txt", buf, sizeof(buf)) < 0);
}

static void test_snapshot_write_roundtrip(void) {
    uint32_t sz = 0;
    char buf[16];
    int n;

    setUp();
    TEST_ASSERT_EQUAL(4, overlay_write("k.txt", "v7ok", 4));
    TEST_ASSERT_EQUAL(0, overlay_snapshot(g_snap, sizeof(g_snap), &sz));
    overlay_init();
    TEST_ASSERT_TRUE(overlay_read("k.txt", buf, sizeof(buf)) < 0);
    TEST_ASSERT_EQUAL(0, overlay_restore(g_snap, sz));
    n = overlay_read("k.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL(4, n);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("v7ok", buf);
}

static void test_snapshot_mkdir_roundtrip(void) {
    uint32_t sz = 0;
    os_dirent_t st;
    char buf[16];
    int n;

    setUp();
    TEST_ASSERT_EQUAL(OV_OK, overlay_mkdir("qd"));
    TEST_ASSERT_EQUAL(5, overlay_write("qd/k.txt", "hello", 5));
    TEST_ASSERT_EQUAL(0, overlay_snapshot(g_snap, sizeof(g_snap), &sz));
    overlay_init();
    TEST_ASSERT_EQUAL(0, overlay_restore(g_snap, sz));
    TEST_ASSERT_TRUE(overlay_is_dir("qd"));
    TEST_ASSERT_EQUAL(OV_OK, overlay_stat("qd", &st));
    TEST_ASSERT_EQUAL(OS_DIRENT_DIR, st.flags);
    n = overlay_read("qd/k.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL(5, n);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("hello", buf);
}

static void test_snapshot_unlink_roundtrip(void) {
    uint32_t sz_before = 0;
    uint32_t sz_after = 0;
    uint8_t snap_after[OV_SNAP_SIZE];
    char buf[16];

    setUp();
    TEST_ASSERT_EQUAL(3, overlay_write("gone.txt", "bye", 3));
    TEST_ASSERT_EQUAL(0, overlay_snapshot(g_snap, sizeof(g_snap), &sz_before));
    TEST_ASSERT_EQUAL(OV_OK, overlay_unlink("gone.txt"));
    TEST_ASSERT_EQUAL(0, overlay_snapshot(snap_after, sizeof(snap_after), &sz_after));

    TEST_ASSERT_EQUAL(0, overlay_restore(g_snap, sz_before));
    TEST_ASSERT_EQUAL(3, overlay_read("gone.txt", buf, sizeof(buf)));

    TEST_ASSERT_EQUAL(0, overlay_restore(snap_after, sz_after));
    TEST_ASSERT_TRUE(overlay_read("gone.txt", buf, sizeof(buf)) < 0);
}

static void test_restore_rejects_bad_magic(void) {
    uint32_t sz = 0;
    char buf[16];
    int n;

    setUp();
    TEST_ASSERT_EQUAL(4, overlay_write("stay.txt", "keep", 4));
    TEST_ASSERT_EQUAL(0, overlay_snapshot(g_snap, sizeof(g_snap), &sz));
    g_snap[0] ^= 0xFF;
    TEST_ASSERT_TRUE(overlay_restore(g_snap, sz) < 0);
    n = overlay_read("stay.txt", buf, sizeof(buf));
    TEST_ASSERT_EQUAL(4, n);
    buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING("keep", buf);
}

static void test_snapshot_rejects_small_buffer(void) {
    uint8_t tiny[16];
    uint32_t sz = 99;

    setUp();
    TEST_ASSERT_TRUE(overlay_snapshot(tiny, sizeof(tiny), &sz) < 0);
    TEST_ASSERT_TRUE(overlay_restore(tiny, sizeof(tiny)) < 0);
    TEST_ASSERT_EQUAL(99, sz);
}

int main(void) {
    unity_init();
    RUN_TEST(test_snapshot_empty_roundtrip);
    RUN_TEST(test_snapshot_write_roundtrip);
    RUN_TEST(test_snapshot_mkdir_roundtrip);
    RUN_TEST(test_snapshot_unlink_roundtrip);
    RUN_TEST(test_restore_rejects_bad_magic);
    RUN_TEST(test_snapshot_rejects_small_buffer);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
