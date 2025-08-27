#include "idt.h"
#include "keyboard.h"
#include "timer.h"

// Déclaration pour le handler du clavier en C
void keyboard_interrupt_handler();

// Fonctions externes pour les ports I/O (définies dans kernel.c)
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void print_string_serial(const char* str);

// Externe pour les ISR qui seront définies en assembleur
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();
extern void irq0(); // ISR pour le timer
extern void irq1(); // ISR pour le clavier
extern void isr_syscall(); // ISR pour les appels système
extern void isr_schedule(); // ISR pour le scheduling volontaire

// Structure pour les registres passés par l'ISR stub
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t;

// C-level fault handler
void fault_handler_c(registers_t *r) {
    print_string_serial("\n!!! KERNEL EXCEPTION !!!\n");
    print_string_serial("Interrupt: ");
    print_hex_serial(r->int_no);
    print_string_serial("\nError Code: ");
    print_hex_serial(r->err_code);
    print_string_serial("\n");

    if (r->int_no == 14) { // Page Fault
        uint32_t faulting_address;
        asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
        print_string_serial("Page Fault at address ");
        print_hex_serial(faulting_address);
        print_string_serial("\n");
    }

    print_string_serial("System Halted.\n");
    for(;;);
}

// Type pour les handlers d'interruption
typedef void (*interrupt_handler_t)();

// Table des handlers d'interruption
static interrupt_handler_t interrupt_handlers[256];

// Fonction pour remapper le PIC
// Par défaut, les IRQs du PIC (0-15) entrent en conflit avec les exceptions CPU.
// On les décale vers les entrées IDT 32-47.
void pic_remap() {
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
    
    // Autorise le timer (IRQ0) et le clavier (IRQ1)
    outb(0x21, 0xFC);   // 11111100b : IRQ0, IRQ1 démasquées
    outb(0xA1, 0xFF);
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

void install_exception_handlers() {
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
}

// Initialise toutes nos interruptions
void interrupts_init() {
    print_string_serial("Initialisation du systeme d'interruptions...\n");
    
    install_exception_handlers();

    // Initialise la table des handlers
    for (int i = 0; i < 256; i++) {
        interrupt_handlers[i] = 0;
    }
    
    pic_remap();

    // Enregistre les handlers
    register_interrupt_handler(32, timer_handler);    // IRQ 0 - Timer
    register_interrupt_handler(33, keyboard_interrupt_handler); // IRQ 1 - Clavier
    
    // Associe les entrées de l'IDT aux routines assembleur
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);        // Timer
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);        // Clavier
    idt_set_gate(0x30, (uint32_t)isr_schedule, 0x08, 0xEE); // Scheduler (Ring 3)
    idt_set_gate(0x80, (uint32_t)isr_syscall, 0x08, 0xEE); // Syscalls (Ring 3 accessible)

    // Activer les interruptions sur le CPU
    asm volatile ("sti");
    
    print_string_serial("Systeme d'interruptions initialise.\n");
}

