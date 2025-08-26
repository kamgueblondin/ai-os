#include "keyboard.h"
#include "pic.h"
#include "isr.h"
#include "io.h"
#include "spinlock.h"

// From the new implementation
#define KBD_DATA 0x60
#define BUF_SZ   1024

static volatile char rb[BUF_SZ];
static volatile int rhead = 0, rtail = 0;
static spinlock_t rb_lock = 0;

// Scancode set 1 for US QWERTY layout.
static const char scancode_set1[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// --- New Buffer Logic ---
static inline void rb_push(char c) {
    spin_lock(&rb_lock);
    int next_head = (rhead + 1) % BUF_SZ;
    if (next_head != rtail) {
        rb[rhead] = c;
        rhead = next_head;
    }
    spin_unlock(&rb_lock);
}

int kbd_pop_char(void) {
    int c = -1;
    spin_lock(&rb_lock);
    if (rhead != rtail) {
        c = (unsigned char)rb[rtail];
        rtail = (rtail + 1) % BUF_SZ;
    }
    spin_unlock(&rb_lock);
    return c;
}

// --- New IRQ Handler ---
static int is_break_code(uint8_t scancode) {
    return scancode & 0x80;
}

void keyboard_interrupt_handler(void) {
    uint8_t scancode = inb(KBD_DATA);
    if (!is_break_code(scancode)) {
        uint8_t code = scancode & 0x7F;
        if (code < 128) {
            char ch = scancode_set1[code];
            if (ch) {
                rb_push(ch);
            }
        }
    }
    // EOI is sent by the assembly stub (irq1), so it's removed from here.
}


// --- PS/2 Controller Initialization (restored from original file) ---
static void keyboard_wait_for_input() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 1) == 0);
}

static void keyboard_wait_for_output() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 2) != 0);
}

static void keyboard_send_command(uint8_t cmd) {
    keyboard_wait_for_output();
    outb(0x64, cmd);
}

static void keyboard_send_data(uint8_t data) {
    keyboard_wait_for_output();
    outb(0x60, data);
}

static uint8_t keyboard_read_data() {
    keyboard_wait_for_input();
    return inb(0x60);
}

// --- Combined Initialization ---
void keyboard_init(void) {
    // 1. Hardware initialization for the PS/2 controller
    keyboard_send_command(0xAD); // Disable first PS/2 port
    keyboard_send_command(0xA7); // Disable second PS/2 port

    // Flush output buffer
    while (inb(0x64) & 1) {
        inb(0x60);
    }

    // Set controller configuration byte
    keyboard_send_command(0x20); // Read config byte
    uint8_t config = keyboard_read_data();
    config |= 1;     // Enable interrupt for port 1
    config &= ~0x10; // Disable PS2 port 2 clock
    config &= ~0x40; // Disable translation
    keyboard_send_command(0x60); // Write config byte
    keyboard_send_data(config);

    // Enable device
    keyboard_send_command(0xAE); // Enable first PS/2 port

    // Reset device
    keyboard_send_data(0xFF);
    keyboard_read_data(); // Wait for ACK

    // 2. Software initialization for the interrupt handler
    isr_register_irq(1, keyboard_interrupt_handler);
    pic_unmask(1);
}
