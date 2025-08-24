#include "kbd_buffer.h"
#include <stdint.h>

#define KBD_BUF_SZ 256
static volatile uint8_t buf[KBD_BUF_SZ];
static volatile unsigned int head = 0;
static volatile unsigned int tail = 0;

void kbd_push_scancode(uint8_t s){
    unsigned int h = __atomic_load_n(&head, __ATOMIC_RELAXED);
    buf[h % KBD_BUF_SZ] = s;
    __atomic_store_n(&head, h + 1, __ATOMIC_RELEASE);
}

int kbd_pop_scancode(uint8_t *out){
    for(;;){
        unsigned int t = __atomic_load_n(&tail, __ATOMIC_ACQUIRE);
        unsigned int h = __atomic_load_n(&head, __ATOMIC_ACQUIRE);
        if (t != h){
            *out = buf[t % KBD_BUF_SZ];
            __atomic_store_n(&tail, t + 1, __ATOMIC_RELEASE);
            return 1;
        }
        // Pas de timer ? on attend une IRQ avec HLT
        __asm__ __volatile__("sti; hlt");
    }
}
