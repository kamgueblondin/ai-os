#include "../../framework/unity.h"
#include "../../../kernel/pci.h"

void setUp(void) {}
void tearDown(void) {}

void test_config_address_masks_fields_and_aligns_offset(void) {
    TEST_ASSERT_EQUAL(0x80000000U | (2U << 16) | (3U << 11) |
                      (1U << 8) | 0x20U,
                      pci_config_address(2, 3, 1, 0x23));
}

void test_decode_id_extracts_vendor_and_device(void) {
    pci_device_t device = {0};
    pci_decode_id(0x813910ecU, &device);
    TEST_ASSERT_EQUAL(0x10ec, device.vendor_id);
    TEST_ASSERT_EQUAL(0x8139, device.device_id);
}

int main(void) {
    unity_init();
    RUN_TEST(test_config_address_masks_fields_and_aligns_offset);
    RUN_TEST(test_decode_id_extracts_vendor_and_device);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
