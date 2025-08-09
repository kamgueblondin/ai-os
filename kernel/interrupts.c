#include "idt.h"
#include "keyboard.h"

// Fonctions externes pour les ports I/O (définies dans kernel.c)
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

// Externe pour les ISR qui seront définies en assembleur
extern void irq1(); // ISR pour le clavier

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

// ISR commune pour les interruptions
void interrupt_handler(void* frame) {
    // Pour l'instant, on ne fait rien
}

// Initialise toutes nos interruptions
void interrupts_init() {
    pic_remap();

    // Associe l'entrée 33 (IRQ 1) de l'IDT à la routine du clavier
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);

    // Activer les interruptions sur le CPU
    asm volatile ("sti");
}

