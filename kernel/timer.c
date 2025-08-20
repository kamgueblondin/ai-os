#include "timer.h"
#include "task/task.h"

// Fonctions externes
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
extern void print_string_serial(const char* str);

// Variables globales
uint32_t timer_ticks = 0;
uint32_t software_timer_counter = 0;
int timer_mode = 0; // 0 = logiciel, 1 = matériel

// Timer logiciel de secours
void software_timer_tick() {
    software_timer_counter++;
    
    // Simule un tick timer toutes les 100000 itérations (approximativement)
    if (software_timer_counter % 100000 == 0) {
        timer_ticks++;
        
        // Log périodique pour monitoring
        if (timer_ticks % 10 == 0) {
            print_string_serial("S"); // S pour Software timer
        }
        
        // PHASE 4: Réactivation progressive du multitâche
        // Appel conditionnel à l'ordonnanceur si activé
        if (timer_ticks > 50) { // Attendre 50 ticks avant d'activer le multitâche
            // schedule(); // À activer quand l'ordonnanceur sera stable
        }
    }
}

// Handler appelé par l'ISR du timer matériel
void timer_handler() {
    timer_ticks++;
    
    // Log périodique pour monitoring
    if (timer_ticks % 10 == 0) {
        print_string_serial("H"); // H pour Hardware timer
    }
    
    // Appel conditionnel à l'ordonnanceur si activé
    // schedule(); // À activer en Phase 4
}

// Fonction unifiée pour obtenir les ticks (marche avec les deux modes)
uint32_t timer_get_ticks() {
    return timer_ticks;
}

// Fonction pour mettre à jour le timer (à appeler régulièrement)
void timer_update() {
    if (timer_mode == 0) {
        software_timer_tick();
    }
    // En mode matériel, les ticks sont gérés par l'ISR
}

// Initialise le timer (mode logiciel forcé pour stabilité)
void timer_init(uint32_t frequency) {
    print_string_serial("Initialisation du timer hybride...\n");
    
    // SOLUTION DEFINITIVE: Forcer le mode logiciel pour éviter les redémarrages
    print_string_serial("Mode timer logiciel force pour stabilite...\n");
    
    timer_mode = 0; // Forcer le mode logiciel
    
    // Masquer explicitement l'IRQ0 pour éviter les interruptions timer matériel
    asm volatile("cli");
    unsigned char mask = inb(0x21);
    mask |= (1 << 0); // Masquer IRQ0
    outb(0x21, mask);
    asm volatile("sti");
    
    print_string_serial("Timer logiciel actif et stable!\n");
    print_string_serial("Frequence simulee: ");
    
    // Affichage de la fréquence
    char freq_str[16];
    int i = 0;
    uint32_t temp = frequency;
    if (temp == 0) {
        freq_str[i++] = '0';
    } else {
        while (temp > 0) {
            freq_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    freq_str[i] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = freq_str[j];
        freq_str[j] = freq_str[i - 1 - j];
        freq_str[i - 1 - j] = tmp;
    }
    
    print_string_serial(freq_str);
    print_string_serial(" Hz (logiciel)\n");
}

// Attend un certain nombre de ticks
void timer_wait(uint32_t ticks) {
    uint32_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        asm volatile("hlt"); // Attend la prochaine interruption
    }
}

void init_scheduler_timer(uint32_t frequency) {
    // Le PIT (Programmable Interval Timer) utilise une fréquence de base de 1.193182 MHz
    uint32_t divisor = 1193182 / frequency;

    // Envoie l'octet de commande pour le canal 0
    // 0x36 = 00110110b
    // Bits 6-7: 00 = Canal 0
    // Bits 4-5: 11 = Accès LSB puis MSB
    // Bits 1-3: 010 = Mode 2 (rate generator)
    // Bit 0:    0 = Compteur binaire 16 bits
    outb(0x43, 0x36);

    // Envoie le diviseur (partie basse puis haute)
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);
    outb(0x40, l);
    outb(0x40, h);

    // Active l'IRQ0 (timer)
    asm volatile("cli");
    unsigned char mask = inb(0x21);
    mask &= ~(1 << 0); // Démasquer IRQ0
    outb(0x21, mask);
    asm volatile("sti");

    timer_mode = 1; // Mode matériel
    print_string_serial("Timer materiel active pour le scheduler.\n");
}
