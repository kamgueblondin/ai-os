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
    asm volatile("int $0x80" : : "a"(4), "b"(buffer), "c"(size)); 
}

// SYS_EXEC - Nouveau syscall pour exécuter un programme
int exec(const char* path, char* argv[]) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(5), "b"(path), "c"(argv));
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
    print_string("========================================\n");
    print_string("    AI-OS Shell v1.0 - Bienvenue !     \n");
    print_string("========================================\n");
    print_string("Systeme d'exploitation avec IA integree\n");
    print_string("Tapez vos questions et l'IA repondra.\n");
    print_string("Commandes speciales :\n");
    print_string("  'aide' - Afficher l'aide\n");
    print_string("  'exit' - Quitter le shell\n");
    print_string("  'clear' - Effacer l'ecran\n");
    print_string("  'about' - A propos d'AI-OS\n");
    print_string("========================================\n");
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
    print_string("AI-OS v4.0 - Systeme d'exploitation experimental\n");
    print_string("Concu pour l'integration d'intelligence artificielle\n");
    print_string("Fonctionnalites :\n");
    print_string("- Multitache preemptif\n");
    print_string("- Gestion memoire avancee (PMM/VMM)\n");
    print_string("- Systeme de fichiers initrd\n");
    print_string("- Isolation Ring 0/3\n");
    print_string("- Shell interactif avec IA simulee\n");
    print_string("Developpe avec passion pour l'avenir de l'IA !\n\n");
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
    else if (strcmp(input, "help") == 0) {
        print_string("\n=== Aide du Shell AI-OS ===\n");
        print_string("Commandes internes :\n");
        print_string("  exit/quit - Quitter le shell\n");
        print_string("  clear/cls - Effacer l'ecran\n");
        print_string("  about/version - Informations sur AI-OS\n");
        print_string("  help - Cette aide\n");
        print_string("\nPour toute autre question, l'IA repondra automatiquement.\n");
        print_string("Exemple : 'bonjour', 'quelle heure est-il ?', 'aide'\n\n");
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

