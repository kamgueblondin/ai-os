#pragma once

#include "interrupts.h" // for register_interrupt_handler

// The IRQs are remapped to start at interrupt 32.
#define IRQ_BASE 32

// Compatibility wrapper for registering an IRQ handler.
// The handler function must have the signature: void handler(void);
static inline void isr_register_irq(int irq, void (*handler)()) {
    register_interrupt_handler(irq + IRQ_BASE, handler);
}
