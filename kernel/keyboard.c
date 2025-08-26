#include "keyboard.h"
#include "pic.h"
#include "isr.h"
#include "io.h"
#include "spinlock.h"
#include <stdint.h>

/* Prototypes for the serial functions already present in the tree.
   We declare them here to avoid implicit declaration warnings. */
void print_string_serial(const char *s);
void print_hex_serial(uint32_t v);

#define KBD_DATA 0x60
#define BUF_SZ   1024

static volatile char rb[BUF_SZ];
static volatile int rhead = 0, rtail = 0;
static spinlock_t rb_lock = 0;

static const char scancode_set1[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',       // 0x00 - 0x0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',    // 0x10 - 0x1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',   // 0x20 - 0x2F
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,                   // 0x30 - 0x3F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x40 - 0x4F
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1', // 0x50 - 0x5F
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x60 - 0x6F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // 0x70 - 0x7F
};

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

static int is_break_code(uint8_t scancode) {
    return scancode & 0x80;
}

void keyboard_interrupt_handler(void) {
    /* Debug: print a marker and the scancode we just read */
    print_string_serial("[K]");
    uint8_t sc = inb(0x60);
    print_string_serial(" sc=");
    print_hex_serial((uint32_t)sc);
    print_string_serial("\n");

    /* If key down (high bit clear) push to ring buffer and notify */
    if(!(sc & 0x80)){
        char ch = scancode_set1[sc & 0x7F];
        if(ch){
            rb_push(ch);
            print_string_serial(">"); /* show pushed char */
            if((unsigned char)ch >= 0x20 && (unsigned char)ch < 0x7F){
                char s[2] = { ch, 0 };
                print_string_serial(s);
            } else {
                print_hex_serial((uint32_t)ch);
            }
            print_string_serial("\n");
        }
    }
    outb(0x20, 0x20); // EOI
}

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

void keyboard_init(void) {
    keyboard_send_command(0xAD);
    keyboard_send_command(0xA7);
    while (inb(0x64) & 1) { inb(0x60); }
    keyboard_send_command(0x20);
    uint8_t config = keyboard_read_data();
    config |= 1;
    config &= ~0x10;
    config &= ~0x40;
    keyboard_send_command(0x60);
    keyboard_send_data(config);
    keyboard_send_command(0xAE);
    keyboard_send_data(0xFF);
    keyboard_read_data();
    isr_register_irq(1, keyboard_interrupt_handler);
    pic_unmask(1);
}
