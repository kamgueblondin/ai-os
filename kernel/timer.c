#include "timer.h"
#include "task/task.h"

// Fonctions externes
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
extern void print_string_serial(const char* str);

// Variables globales
uint32_t timer_ticks = 0;

// Handler appelé par l'ISR du timer
void timer_handler() {
    timer_ticks++;
    
    // Appelle l'ordonnanceur pour un changement de tâche potentiel
    schedule();
}

// Initialise le PIT pour générer des interruptions à la fréquence donnée
void timer_init(uint32_t frequency) {
    print_string_serial("Initialisation du timer systeme...\n");
    
    // Calcule le diviseur pour la fréquence désirée
    // Le PIT fonctionne à 1193180 Hz par défaut
    uint32_t divisor = 1193180 / frequency;
    
    // Assure-toi que le diviseur est dans les limites
    if (divisor > 65535) {
        divisor = 65535;
    }
    if (divisor < 1) {
        divisor = 1;
    }
    
    // Configure le PIT
    // Sélectionne le canal 0, accès low/high byte, mode onde carrée
    outb(PIT_COMMAND, PIT_CHANNEL_0_SELECT | PIT_ACCESS_LOHI | PIT_MODE_SQUARE_WAVE);
    
    // Envoie le diviseur (low byte puis high byte)
    outb(PIT_CHANNEL_0, divisor & 0xFF);
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);
    
    // Le timer est maintenant configuré, mais il faut encore enregistrer
    // le handler d'interruption dans le système d'interruptions
    
    print_string_serial("Timer configure pour ");
    
    // Affichage de la fréquence (conversion simple)
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
    print_string_serial(" Hz\n");
}

// Obtient le nombre de ticks depuis le démarrage
uint32_t timer_get_ticks() {
    return timer_ticks;
}

// Attend un certain nombre de ticks
void timer_wait(uint32_t ticks) {
    uint32_t start_ticks = timer_ticks;
    while (timer_ticks < start_ticks + ticks) {
        asm volatile("hlt"); // Attend la prochaine interruption
    }
}

