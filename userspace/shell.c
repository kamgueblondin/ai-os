// shell.c - Shell Interactif pour AI-OS
// Interface utilisateur principale avec intégration du simulateur d'IA

// Wrappers pour les appels système
void putc(char c) { 
    asm volatile("int $0x80" : : "a"(1), "b"(c)); 
}

void exit_program() {
    asm volatile("int $0x80" : : "a"(0));
}

// SYS_GETS - Nouveau syscall pour lire une ligne depuis le clavier
void gets(char* buffer, int size) { 
    asm volatile("int $0x80" : : "a"(5), "b"(buffer), "c"(size)); 
}

// SYS_EXEC - Nouveau syscall pour exécuter un programme
int exec(const char* path, char* argv[]) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(6), "b"(path), "c"(argv));
    return result;
}

// Fonctions utilitaires
int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        i++;
    }
    return s1[i] - s2[i];
}

void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Fonction pour afficher une chaîne
void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putc(str[i]);
    }
}

// Fonction pour afficher le banner de bienvenue
void show_banner() {
    print_string("\n");
    print_string("==========================================\n");
    print_string("    AI-OS Shell v5.0 - Bienvenue !       \n");
    print_string("==========================================\n");
    print_string("Systeme d'exploitation avec IA integree\n");
    print_string("Nouvelles commandes disponibles :\n");
    print_string("  'ls' - Lister les fichiers\n");
    print_string("  'ps' - Voir les processus\n");
    print_string("  'mem' - Informations memoire\n");
    print_string("  'sysinfo' - Infos systeme\n");
    print_string("  'cat <fichier>' - Lire un fichier\n");
    print_string("  'help' - Aide complete\n");
    print_string("  'exit' - Quitter\n");
    print_string("\nPosez aussi vos questions a l'IA !\n");
    print_string("==========================================\n");
    print_string("\n");
}

// Fonction pour effacer l'écran (simulation)
void clear_screen() {
    // Afficher plusieurs lignes vides pour simuler un effacement
    for (int i = 0; i < 25; i++) {
        print_string("\n");
    }
    print_string("Ecran efface.\n\n");
}

// Fonction pour afficher les informations sur AI-OS
void show_about() {
    print_string("\n=== A propos d'AI-OS ===\n");
    print_string("AI-OS v5.0 - Systeme d'exploitation experimental\n");
    print_string("Concu pour l'integration d'intelligence artificielle\n");
    print_string("Fonctionnalites :\n");
    print_string("- Multitache preemptif\n");
    print_string("- Gestion memoire avancee (PMM/VMM)\n");
    print_string("- Systeme de fichiers initrd\n");
    print_string("- Isolation Ring 0/3\n");
    print_string("- Shell interactif avec IA simulee\n");
    print_string("- Commandes systeme etendues\n");
    print_string("Developpe avec passion pour l'avenir de l'IA !\n\n");
}

// Fonction pour lister les fichiers (simulation)
void list_files() {
    print_string("\n=== Fichiers disponibles dans l'initrd ===\n");
    print_string("test.txt        - Fichier de test\n");
    print_string("hello.txt       - Fichier de demonstration\n");
    print_string("config.cfg      - Configuration systeme\n");
    print_string("startup.sh      - Script de demarrage\n");
    print_string("ai_data.txt     - Donnees IA\n");
    print_string("ai_knowledge.txt- Base de connaissances IA\n");
    print_string("shell           - Shell interactif (executable)\n");
    print_string("fake_ai         - Simulateur IA (executable)\n");
    print_string("user_program    - Programme de test (executable)\n");
    print_string("\n");
}

// Fonction pour afficher le contenu d'un fichier (simulation)
void show_file_content(const char* filename) {
    print_string("\n=== Contenu de ");
    print_string(filename);
    print_string(" ===\n");
    
    if (strcmp(filename, "test.txt") == 0) {
        print_string("Ceci est un fichier de test depuis l'initrd !\n");
    }
    else if (strcmp(filename, "hello.txt") == 0) {
        print_string("Un autre fichier de demonstration.\n");
    }
    else if (strcmp(filename, "config.cfg") == 0) {
        print_string("Configuration du systeme AI-OS v5.0\n");
    }
    else if (strcmp(filename, "ai_data.txt") == 0) {
        print_string("Donnees pour l'intelligence artificielle simulee\n");
    }
    else if (strcmp(filename, "ai_knowledge.txt") == 0) {
        print_string("Base de connaissances IA - Version simulation\n");
    }
    else {
        print_string("Erreur: Fichier non trouve ou non lisible.\n");
    }
    print_string("\n");
}

// Fonction pour afficher les informations système
void show_system_info() {
    print_string("\n=== Informations Systeme AI-OS ===\n");
    print_string("Version: AI-OS v5.0\n");
    print_string("Architecture: x86 32-bit\n");
    print_string("Memoire: ~128MB detectee\n");
    print_string("Pages: 32,895 pages de 4KB\n");
    print_string("Multitache: Actif (Round-Robin 100Hz)\n");
    print_string("Espace utilisateur: Ring 3 securise\n");
    print_string("Syscalls: 7 appels systeme disponibles\n");
    print_string("Uptime: Depuis le demarrage\n");
    print_string("\n");
}

// Fonction pour afficher les processus (simulation)
void show_processes() {
    print_string("\n=== Processus Actifs ===\n");
    print_string("PID  ETAT    PROGRAMME\n");
    print_string("---  ------  ---------\n");
    print_string("1    RUN     kernel (idle)\n");
    print_string("2    RUN     shell (actuel)\n");
    print_string("3    READY   fake_ai\n");
    print_string("4    READY   user_program\n");
    print_string("\nTotal: 4 taches systeme\n");
    print_string("Ordonnanceur: Round-Robin preemptif\n");
    print_string("\n");
}

// Fonction pour afficher l'utilisation mémoire
void show_memory_usage() {
    print_string("\n=== Utilisation Memoire ===\n");
    print_string("Memoire totale: 128MB\n");
    print_string("Pages totales: 32,895 pages\n");
    print_string("Taille page: 4KB\n");
    print_string("Memoire noyau: ~20KB\n");
    print_string("Memoire utilisateur: Variable\n");
    print_string("PMM: Actif (bitmap)\n");
    print_string("VMM: Actif (paging)\n");
    print_string("Protection: Ring 0/3\n");
    print_string("\n");
}

// Fonction pour afficher la date/heure (simulation)
void show_datetime() {
    print_string("\n=== Date et Heure ===\n");
    print_string("AI-OS ne dispose pas encore d'horloge RTC.\n");
    print_string("Uptime: Depuis le demarrage du systeme\n");
    print_string("Timer: PIT configure a 100Hz\n");
    print_string("Ticks: En cours de comptage...\n");
    print_string("\n");
}

// Fonction pour traiter les commandes internes du shell
int handle_internal_command(const char* input) {
    if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
        print_string("Au revoir ! Merci d'avoir utilise AI-OS.\n");
        exit_program();
        return 1; // Ne sera jamais atteint
    }
    else if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) {
        clear_screen();
        return 1;
    }
    else if (strcmp(input, "about") == 0 || strcmp(input, "version") == 0) {
        show_about();
        return 1;
    }
    else if (strcmp(input, "ls") == 0 || strcmp(input, "dir") == 0) {
        list_files();
        return 1;
    }
    else if (strcmp(input, "ps") == 0 || strcmp(input, "tasks") == 0) {
        show_processes();
        return 1;
    }
    else if (strcmp(input, "mem") == 0 || strcmp(input, "memory") == 0) {
        show_memory_usage();
        return 1;
    }
    else if (strcmp(input, "sysinfo") == 0 || strcmp(input, "info") == 0) {
        show_system_info();
        return 1;
    }
    else if (strcmp(input, "date") == 0 || strcmp(input, "time") == 0) {
        show_datetime();
        return 1;
    }
    // Commande cat avec argument (format: "cat filename")
    else if (input[0] == 'c' && input[1] == 'a' && input[2] == 't' && input[3] == ' ') {
        const char* filename = input + 4; // Ignorer "cat "
        show_file_content(filename);
        return 1;
    }
    else if (strcmp(input, "help") == 0) {
        print_string("\n=== Aide du Shell AI-OS v5.0 ===\n");
        print_string("Commandes systeme :\n");
        print_string("  ls/dir        - Lister les fichiers\n");
        print_string("  cat <fichier> - Afficher le contenu d'un fichier\n");
        print_string("  ps/tasks      - Afficher les processus\n");
        print_string("  mem/memory    - Informations memoire\n");
        print_string("  sysinfo/info  - Informations systeme\n");
        print_string("  date/time     - Date et heure\n");
        print_string("\nCommandes internes :\n");
        print_string("  exit/quit     - Quitter le shell\n");
        print_string("  clear/cls     - Effacer l'ecran\n");
        print_string("  about/version - A propos d'AI-OS\n");
        print_string("  help          - Cette aide\n");
        print_string("\nPour toute autre question, l'IA repondra automatiquement.\n");
        print_string("Exemple : 'bonjour', 'comment ca va ?', 'explique-moi l'IA'\n\n");
        return 1;
    }
    
    return 0; // Commande non interne
}

// Fonction pour exécuter l'IA avec la question de l'utilisateur
void execute_ai(const char* query) {
    // Préparer les arguments pour fake_ai
    char* argv[3];
    argv[0] = "fake_ai";     // Nom du programme
    argv[1] = (char*)query;  // La question de l'utilisateur
    argv[2] = 0;             // Terminateur NULL
    
    // Exécuter le programme fake_ai
    int result = exec("fake_ai", argv);
    
    if (result != 0) {
        print_string("Erreur: Impossible de lancer l'IA simulee.\n");
        print_string("Verifiez que 'fake_ai' est present dans l'initrd.\n");
    }
}

// Boucle principale du shell
void shell_loop() {
    char input_buffer[256];
    
    while (1) {
        // Afficher le prompt
        print_string("AI-OS> ");
        
        // Lire l'entrée utilisateur
        gets(input_buffer, 255);
        
        // Ignorer les lignes vides
        if (strlen(input_buffer) == 0) {
            continue;
        }
        
        // Traiter les commandes internes du shell
        if (handle_internal_command(input_buffer)) {
            continue;
        }
        
        // Sinon, passer la question à l'IA
        print_string("\n[IA] ");
        execute_ai(input_buffer);
        print_string("\n");
    }
}

// Point d'entrée principal du shell
void main() {
    // Afficher le banner de bienvenue
    show_banner();
    
    // Démarrer la boucle interactive
    shell_loop();
    
    // Ne devrait jamais être atteint
    exit_program();
}
