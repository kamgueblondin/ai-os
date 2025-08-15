#include "idt.h"
#include "keyboard.h"
#include "timer.h"

// Fonctions externes pour les ports I/O (définies dans kernel.c)
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void print_string_serial(const char* str);

// Externe pour les ISR qui seront définies en assembleur
extern void irq0(); // ISR pour le timer
extern void irq1(); // ISR pour le clavier
extern void isr_syscall(); // ISR pour les appels système

// Type pour les handlers d'interruption
typedef void (*interrupt_handler_t)();

// Table des handlers d'interruption
static interrupt_handler_t interrupt_handlers[256];

// Fonction pour remapper le PIC
// Par défaut, les IRQs du PIC (0-15) entrent en conflit avec les exceptions CPU.
// On les décale vers les entrées IDT 32-47.
void pic_remap() {
    unsigned char a1, a2;
    
    // Sauvegarde les masques
    a1 = inb(0x21);
    a2 = inb(0xA1);
    
    // Démarre la séquence d'initialisation (en mode cascade)
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    
    // PIC1: offset de vecteur 0x20 (32)
    outb(0x21, 0x20);
    // PIC2: offset de vecteur 0x28 (40)
    outb(0xA1, 0x28);
    
    // PIC1: il y a un esclave PIC à IRQ2 (0000 0100)
    outb(0x21, 0x04);
    // PIC2: son identité en cascade (0000 0010)
    outb(0xA1, 0x02);
    
    // Mode 8086
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Restaure les masques
    outb(0x21, a1);
    outb(0xA1, a2);
}

// Fonction pour envoyer EOI (End of Interrupt)
void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

// Enregistre un handler pour une interruption donnée
void register_interrupt_handler(unsigned char interrupt, interrupt_handler_t handler) {
    interrupt_handlers[interrupt] = handler;
}

// ISR commune pour les interruptions
void interrupt_handler(unsigned char interrupt_number) {
    // Appelle le handler spécifique s'il existe
    if (interrupt_handlers[interrupt_number]) {
        interrupt_handlers[interrupt_number]();
    }
    
    // Envoie EOI pour les IRQs
    if (interrupt_number >= 32 && interrupt_number < 48) {
        pic_send_eoi(interrupt_number - 32);
    }
}

// Initialise toutes nos interruptions
void interrupts_init() {
    print_string_serial("Initialisation du systeme d'interruptions...\n");
    
    // Initialise la table des handlers
    for (int i = 0; i < 256; i++) {
        interrupt_handlers[i] = 0;
    }
    
    pic_remap();

    // Enregistre les handlers
    register_interrupt_handler(32, timer_handler);    // IRQ 0 - Timer
    register_interrupt_handler(33, keyboard_handler); // IRQ 1 - Clavier
    
    // Associe les entrées de l'IDT aux routines assembleur
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);        // Timer
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);        // Clavier
    idt_set_gate(0x80, (uint32_t)isr_syscall, 0x08, 0xEE); // Syscalls (Ring 3 accessible)

    // Activer les interruptions sur le CPU
    asm volatile ("sti");
    
    print_string_serial("Systeme d'interruptions initialise.\n");
}

