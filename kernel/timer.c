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
void timer_handler(cpu_state_t* cpu) {
    timer_ticks++;
    
    // Log périodique pour vérifier que le timer fonctionne
    if (timer_ticks % 100 == 0) {
        print_string_serial("Timer tick: ");
        // Convertir timer_ticks en string et l'afficher
        char buffer[16];
        int i = 0;
        uint32_t num = timer_ticks;
        if (num == 0) {
            buffer[i++] = '0';
        } else {
            while (num > 0) {
                buffer[i++] = '0' + (num % 10);
                num /= 10;
            }
        }
        buffer[i] = '\0';
        // Inverser la string
        for (int j = 0; j < i/2; j++) {
            char temp = buffer[j];
            buffer[j] = buffer[i-1-j];
            buffer[i-1-j] = temp;
        }
        print_string_serial(buffer);
        print_string_serial("\n");
    }
    
    // Vérifier que le système de tâches est initialisé avant de faire du scheduling
    extern task_t* current_task;
    extern task_t* task_queue;
    
    if (current_task && task_queue && timer_ticks > 10) {
        // Attendre quelques ticks avant d'activer le scheduler pour la stabilité
        schedule(cpu);
    } else {
        // Pas encore prêt pour le scheduling, juste continuer
        if (timer_ticks <= 10) {
            print_string_serial("Timer: Attente avant activation du scheduler...\n");
        }
    }
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

// Initialise le timer matériel (PIT) pour le scheduling préemptif
void timer_init(uint32_t frequency) {
    timer_mode = 1; // Mode matériel
    print_string_serial("Initialisation du timer materiel...\n");

    // Le PIT (Programmable Interval Timer) utilise une fréquence de base de 1.193182 MHz
    uint32_t divisor = 1193182 / frequency;

    // Envoie l'octet de commande pour le canal 0
    // 0x36 = 00110110b -> Canal 0, LSB/MSB, Mode 2 (rate generator)
    outb(0x43, 0x36);

    // Envoie le diviseur
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    
    print_string_serial("Timer materiel active pour le scheduler.\n");
}

// Attend un certain nombre de ticks
void timer_wait(uint32_t ticks) {
    uint32_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        asm volatile("hlt"); // Attend la prochaine interruption
    }
}
