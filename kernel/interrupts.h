#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

// Structure du frame d'interruption poussé par le CPU.
// Pour les interruptions sans code d'erreur.
struct intr_frame {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
};

// Type pour les handlers d'interruption
typedef void (*interrupt_handler_t)();

void pic_remap();
void interrupts_init();
void register_interrupt_handler(unsigned char interrupt, interrupt_handler_t handler);
void interrupt_handler(unsigned char interrupt_number);

#endif

