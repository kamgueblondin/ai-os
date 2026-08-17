#include "../../framework/unity.h"
#include "../../../kernel/ne2k.h"

typedef struct {
    uint8_t reset;
    uint8_t isr;
    uint8_t writes;
} fake_ne2k_t;

void setUp(void) {}
void tearDown(void) {}

static uint8_t fake_inb(void* context, uint16_t port) {
    fake_ne2k_t* fake = (fake_ne2k_t*)context;
    if ((port & 0x1fU) == NE2K_REG_RESET) return fake->reset;
    if ((port & 0x1fU) == NE2K_REG_ISR) return fake->isr;
    return 0U;
}

static void fake_outb(void* context, uint16_t port, uint8_t value) {
    fake_ne2k_t* fake = (fake_ne2k_t*)context;
    fake->writes++;
    if ((port & 0x1fU) == NE2K_REG_RESET) fake->reset = value;
}

void test_probe_and_prepare_use_injected_io(void) {
    fake_ne2k_t fake = {0x12, NE2K_ISR_RESET, 0};
    ne2k_io_t io = {&fake, fake_inb, fake_outb};
    ne2k_device_t device;
    TEST_ASSERT_EQUAL(0, ne2k_probe(&device, 0x300, &io));
    TEST_ASSERT_EQUAL(0, ne2k_prepare(&device, &io));
    TEST_ASSERT_EQUAL(1, device.initialized);
    TEST_ASSERT_GREATER_THAN(2, fake.writes);
}

void test_probe_rejects_missing_reset_ack(void) {
    fake_ne2k_t fake = {0, 0, 0};
    ne2k_io_t io = {&fake, fake_inb, fake_outb};
    ne2k_device_t device;
    TEST_ASSERT_NOT_EQUAL(0, ne2k_probe(&device, 0x300, &io));
}

int main(void) {
    unity_init();
    RUN_TEST(test_probe_and_prepare_use_injected_io);
    RUN_TEST(test_probe_rejects_missing_reset_ack);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
