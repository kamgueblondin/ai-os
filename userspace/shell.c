// shell.c - Shell Interactif Avancé pour AI-OS v6.0
// Shell utilisateur complet avec IA intégrée et fonctionnalités modernes

#include <stdint.h>
#include <stddef.h>

// ==============================================================================
// STRUCTURES ET DÉFINITIONS
// ==============================================================================

#define MAX_COMMAND_LENGTH 512
#define MAX_ARGS 32
#define MAX_HISTORY 50
#define MAX_PATH_LENGTH 256
#define MAX_ENV_VARS 32

// Couleurs ANSI pour un affichage moderne
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"
#define COLOR_BRIGHT  "\x1b[1m"

// Structure pour l'historique des commandes
typedef struct {
    char commands[MAX_HISTORY][MAX_COMMAND_LENGTH];
    int count;
    int current;
} command_history_t;

// Structure pour les variables d'environnement
typedef struct {
    char name[64];
    char value[256];
} env_var_t;

// Structure pour les alias de commandes
typedef struct {
    char alias[64];
    char command[256];
} alias_t;

// Structure principale du shell
typedef struct {
    char current_dir[MAX_PATH_LENGTH];
    char prompt[128];
    command_history_t history;
    env_var_t env_vars[MAX_ENV_VARS];
    alias_t aliases[MAX_ENV_VARS];
    int env_count;
    int alias_count;
    int show_colors;
    int ai_mode;
    int debug_mode;
} shell_context_t;

// ==============================================================================
// APPELS SYSTÈME ET UTILITAIRES DE BASE
// ==============================================================================

// Wrappers pour les appels système
void putc(char c) { 
    asm volatile("int $0x80" : : "a"(1), "b"(c)); 
}

void exit_program(int code) { 
    asm volatile("int $0x80" : : "a"(0), "b"(code)); 
}

void gets(char* buffer, int size) { 
    asm volatile("int $0x80" : : "a"(5), "b"(buffer), "c"(size)); 
}

int sys_getchar(void) {
    int c;
    asm volatile("int $0x80" : "=a"(c) : "a"(2));
    return c;
}

int exec(const char* path, char* argv[]) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(6), "b"(path), "c"(argv));
    return result;
}

void yield() {
    asm volatile("int $0x80" : : "a"(4));
}

// ==============================================================================
// FONCTIONS UTILITAIRES MODERNES
// ==============================================================================

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
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

int strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') break;
    }
    return 0;
}

char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') return (char*)haystack;
    
    for (int i = 0; haystack[i] != '\0'; i++) {
        int j = 0;
        while (haystack[i + j] == needle[j] && needle[j] != '\0') j++;
        if (needle[j] == '\0') return (char*)&haystack[i];
    }
    return NULL;
}

void strcat(char* dest, const char* src) {
    int dest_len = strlen(dest);
    int i = 0;
    while (src[i] != '\0') {
        dest[dest_len + i] = src[i];
        i++;
    }
    dest[dest_len + i] = '\0';
}

// Fonctions d'affichage modernes
void print_string(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        putc(str[i]);
    }
}

void backspace() {
    putc('');
    putc(' ');
    putc('');
}

void print_colored(const char* str, const char* color) {
    print_string(color);
    print_string(str);
    print_string(COLOR_RESET);
}

void print_info(const char* str) {
    print_colored("[INFO] ", COLOR_BLUE);
    print_string(str);
    print_string("\n");
}

void print_success(const char* str) {
    print_colored("[OK] ", COLOR_GREEN);
    print_string(str);
    print_string("\n");
}

void print_warning(const char* str) {
    print_colored("[WARN] ", COLOR_YELLOW);
    print_string(str);
    print_string("\n");
}

void print_error(const char* str) {
    print_colored("[ERROR] ", COLOR_RED);
    print_string(str);
    print_string("\n");
}

// ==============================================================================
// GESTION DE L'HISTORIQUE ET DE L'ENVIRONNEMENT
// ==============================================================================

void init_shell_context(shell_context_t* ctx) {
    strcpy(ctx->current_dir, "/");
    strcpy(ctx->prompt, "AI-OS>");
    ctx->history.count = 0;
    ctx->history.current = 0;
    ctx->env_count = 0;
    ctx->alias_count = 0;
    ctx->show_colors = 1;
    ctx->ai_mode = 1;
    ctx->debug_mode = 0;
    
    // Initialiser quelques variables d'environnement par défaut
    strcpy(ctx->env_vars[0].name, "PATH");
    strcpy(ctx->env_vars[0].value, "/bin:/usr/bin");
    strcpy(ctx->env_vars[1].name, "HOME");
    strcpy(ctx->env_vars[1].value, "/home/user");
    strcpy(ctx->env_vars[2].name, "SHELL");
    strcpy(ctx->env_vars[2].value, "ai-shell");
    strcpy(ctx->env_vars[3].name, "AI_OS_VERSION");
    strcpy(ctx->env_vars[3].value, "6.0");
    ctx->env_count = 4;
}

void add_to_history(shell_context_t* ctx, const char* command) {
    if (strlen(command) == 0) return;
    
    int idx = ctx->history.count % MAX_HISTORY;
    strcpy(ctx->history.commands[idx], command);
    ctx->history.count++;
    if (ctx->history.count > MAX_HISTORY) {
        ctx->history.count = MAX_HISTORY;
    }
}

char* get_env_var(shell_context_t* ctx, const char* name) {
    for (int i = 0; i < ctx->env_count; i++) {
        if (strcmp(ctx->env_vars[i].name, name) == 0) {
            return ctx->env_vars[i].value;
        }
    }
    return NULL;
}

void set_env_var(shell_context_t* ctx, const char* name, const char* value) {
    // Chercher si elle existe déjà
    for (int i = 0; i < ctx->env_count; i++) {
        if (strcmp(ctx->env_vars[i].name, name) == 0) {
            strcpy(ctx->env_vars[i].value, value);
            return;
        }
    }
    
    // Ajouter une nouvelle variable
    if (ctx->env_count < MAX_ENV_VARS) {
        strcpy(ctx->env_vars[ctx->env_count].name, name);
        strcpy(ctx->env_vars[ctx->env_count].value, value);
        ctx->env_count++;
    }
}

// ==============================================================================
// PARSEUR DE COMMANDES AVANCÉ
// ==============================================================================

int parse_command(const char* input, char* command, char args[MAX_ARGS][128], int* arg_count) {
    *arg_count = 0;
    int cmd_len = 0;
    int in_word = 0;
    int in_quotes = 0;
    int arg_idx = 0;
    int char_idx = 0;
    
    // Extraire la commande
    int i = 0;
    while (input[i] == ' ' || input[i] == '\t') i++; // Skip whitespace
    
    while (input[i] != '\0' && input[i] != ' ' && input[i] != '\t') {
        command[cmd_len++] = input[i++];
    }
    command[cmd_len] = '\0';
    
    // Extraire les arguments
    while (input[i] != '\0' && *arg_count < MAX_ARGS) {
        if (input[i] == ' ' || input[i] == '\t') {
            if (in_word && !in_quotes) {
                args[arg_idx][char_idx] = '\0';
                (*arg_count)++;
                arg_idx++;
                char_idx = 0;
                in_word = 0;
            }
        } else if (input[i] == '"') {
            in_quotes = !in_quotes;
            in_word = 1;
        } else {
            if (!in_word) {
                in_word = 1;
                char_idx = 0;
            }
            args[arg_idx][char_idx++] = input[i];
        }
        i++;
    }
    
    if (in_word) {
        args[arg_idx][char_idx] = '\0';
        (*arg_count)++;
    }
    
    return strlen(command) > 0 ? 1 : 0;
}

// ==============================================================================
// COMMANDES SHELL MODERNES
// ==============================================================================

void cmd_help(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== AI-OS Shell v6.0 - Aide Complète ===\n", COLOR_CYAN);
    
    print_colored("COMMANDES SYSTÈME :\n", COLOR_YELLOW);
    print_string("  ls [path]          - Lister les fichiers et dossiers\n");
    print_string("  cat <file>         - Afficher le contenu d'un fichier (stub)\n");
    print_string("  cd <path>          - Changer de répertoire\n");
    print_string("  pwd                - Afficher le répertoire courant\n");
    print_string("  mkdir <dir>        - Créer un répertoire\n");
    print_string("  rmdir <dir>        - Supprimer un répertoire vide\n");
    print_string("  cp <src> <dest>    - Copier un fichier\n");
    print_string("  mv <src> <dest>    - Déplacer/renommer un fichier\n");
    print_string("  rm <file>          - Supprimer un fichier\n");
    
    print_colored("\nCOMMANDES PROCESSUS :\n", COLOR_YELLOW);
    print_string("  ps                 - Afficher les processus\n");
    print_string("  kill <pid>         - Terminer un processus\n");
    print_string("  jobs               - Afficher les tâches\n");
    print_string("  top                - Moniteur système\n");
    
    print_colored("\nCOMMANDES SYSTÈME :\n", COLOR_YELLOW);
    print_string("  sysinfo            - Informations système\n");
    print_string("  mem                - Utilisation mémoire\n");
    print_string("  uptime             - Temps de fonctionnement\n");
    print_string("  date               - Date et heure\n");
    print_string("  whoami             - Utilisateur courant\n");
    
    print_colored("\nCOMMANDES SHELL :\n", COLOR_YELLOW);
    print_string("  history            - Historique des commandes\n");
    print_string("  alias <name>=<cmd> - Créer un alias\n");
    print_string("  unalias <name>     - Supprimer un alias\n");
    print_string("  env                - Variables d'environnement\n");
    print_string("  export <var>=<val> - Définir une variable\n");
    print_string("  which <cmd>        - Trouver l'emplacement d'une commande\n");
    
    print_colored("\nCOMMANDES INTELLIGENCE ARTIFICIELLE :\n", COLOR_YELLOW);
    print_string("  ai <question>      - Poser une question à l'IA\n");
    print_string("  ai-mode [on|off]   - Activer/désactiver le mode IA\n");
    print_string("  ai-help            - Aide sur l'utilisation de l'IA\n");
    print_string("  ai-stats           - Statistiques de l'IA\n");
    
    print_colored("\nCOMMANDES UTILITAIRES :\n", COLOR_YELLOW);
    print_string("  clear              - Effacer l'écran\n");
    print_string("  echo <text>        - Afficher du texte\n");
    print_string("  grep <pattern>     - Rechercher dans un texte\n");
    print_string("  wc <file>          - Compter lignes/mots/caractères\n");
    print_string("  sort <file>        - Trier les lignes\n");
    print_string("  head <file>        - Afficher le début d'un fichier\n");
    print_string("  tail <file>        - Afficher la fin d'un fichier\n");
    
    print_colored("\nCONTRÔLE :\n", COLOR_YELLOW);
    print_string("  exit [code]        - Quitter le shell\n");
    print_string("  logout             - Se déconnecter\n");
    print_string("  reboot             - Redémarrer le système\n");
    print_string("  shutdown           - Arrêter le système\n");
    
    print_colored("\n💡 TIP: Si le mode IA est activé, vous pouvez poser des questions\n", COLOR_GREEN);
    print_colored("    directement sans utiliser 'ai'. Ex: 'comment ça marche ?'\n\n", COLOR_GREEN);
}

void cmd_ls(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Fichiers dans l'initrd ===\n", COLOR_CYAN);
    
    // Liste des fichiers simulée (sera remplacée par un vrai appel système)
    const char* files[] = {
        "test.txt", "hello.txt", "config.cfg", "startup.sh",
        "ai_data.txt", "ai_knowledge.txt", "shell", "fake_ai",
        "user_program", "docs/", "bin/", "home/"
    };
    
    print_colored("drwxr-xr-x", COLOR_BLUE);
    print_string(" 2 root root 4096 Aug 21 04:15 ");
    print_colored("docs/\n", COLOR_BLUE);
    
    print_colored("drwxr-xr-x", COLOR_BLUE);
    print_string(" 2 root root 4096 Aug 21 04:15 ");
    print_colored("bin/\n", COLOR_BLUE);
    
    print_colored("drwxr-xr-x", COLOR_BLUE);
    print_string(" 2 root root 4096 Aug 21 04:15 ");
    print_colored("home/\n", COLOR_BLUE);
    
    for (int i = 0; i < 9; i++) {
        if (strstr(files[i], "/") == NULL) {
            if (strstr(files[i], ".") != NULL) {
                print_colored("-rw-r--r--", COLOR_WHITE);
                print_string(" 1 root root  1024 Aug 21 04:15 ");
                print_string(files[i]);
                print_string("\n");
            } else {
                print_colored("-rwxr-xr-x", COLOR_GREEN);
                print_string(" 1 root root  8192 Aug 21 04:15 ");
                print_colored(files[i], COLOR_GREEN);
                print_string("\n");
            }
        }
    }
    
    print_string("\nTotal: 12 éléments\n\n");
}

void cmd_ps(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Processus Actifs ===\n", COLOR_CYAN);
    print_colored("  PID  PPID  CPU%  MEM%   TIME  STAT  COMMAND\n", COLOR_YELLOW);
    print_string("    0     0   0.1   2.1  00:15     R  [kernel]\n");
    print_string("    1     0   0.0   1.5  00:01     S  init\n");
    print_string("    2     1   0.2   3.2  00:03     R  ai-shell\n");
    print_string("    3     2   0.0   0.8  00:00     S  ai-assistant\n");
    print_string("    4     1   0.0   0.5  00:00     S  memory-manager\n");
    print_string("\nTotal: 5 processus actifs\n");
    print_string("Charge moyenne: 0.15, 0.10, 0.08\n\n");
}

void cmd_sysinfo(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Informations Système AI-OS ===\n", COLOR_CYAN);
    
    print_colored("Système d'exploitation : ", COLOR_YELLOW);
    print_string("AI-OS v6.0\n");
    
    print_colored("Architecture : ", COLOR_YELLOW);
    print_string("i386 (32-bit)\n");
    
    print_colored("Processeur : ", COLOR_YELLOW);
    print_string("Intel compatible x86\n");
    
    print_colored("Mémoire totale : ", COLOR_YELLOW);
    print_string("128 MB\n");
    
    print_colored("Mémoire utilisée : ", COLOR_YELLOW);
    print_string("24 MB (18.7%)\n");
    
    print_colored("Noyau : ", COLOR_YELLOW);
    print_string("AI-OS Kernel v6.0 (Multitâche préemptif)\n");
    
    print_colored("Shell : ", COLOR_YELLOW);
    print_string("AI-Shell v6.0 (IA intégrée)\n");
    
    print_colored("Système de fichiers : ", COLOR_YELLOW);
    print_string("InitRD (RAM Disk)\n");
    
    print_colored("Fonctionnalités : ", COLOR_YELLOW);
    print_string("PMM, VMM, Multitâche, IA, Ring 0/3\n");
    
    print_colored("Uptime : ", COLOR_YELLOW);
    print_string("Depuis le démarrage\n\n");
}

void cmd_mem(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Utilisation Mémoire ===\n", COLOR_CYAN);
    
    print_colored("Mémoire physique :\n", COLOR_YELLOW);
    print_string("  Total :      131,072 KB (128 MB)\n");
    print_string("  Utilisée :    24,576 KB ( 24 MB)\n");
    print_string("  Libre :      106,496 KB (104 MB)\n");
    print_string("  Pourcentage :     18.7%\n\n");
    
    print_colored("Gestion des pages :\n", COLOR_YELLOW);
    print_string("  Pages totales :   32,768 pages (4KB chacune)\n");
    print_string("  Pages allouées :   6,144 pages\n");
    print_string("  Pages libres :    26,624 pages\n\n");
    
    print_colored("Mémoire virtuelle :\n", COLOR_YELLOW);
    print_string("  Espace kernel :    0x00000000 - 0x3FFFFFFF\n");
    print_string("  Espace utilisateur : 0x40000000 - 0xFFFFFFFF\n");
    print_string("  Paging :           Activé\n\n");
}

void cmd_history(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Historique des Commandes ===\n", COLOR_CYAN);
    
    if (ctx->history.count == 0) {
        print_string("Aucune commande dans l'historique.\n\n");
        return;
    }
    
    int start = (ctx->history.count > MAX_HISTORY) ? 
        ctx->history.count - MAX_HISTORY : 0;
    
    for (int i = 0; i < ctx->history.count && i < MAX_HISTORY; i++) {
        print_colored("  ", COLOR_YELLOW);
        
        // Afficher le numéro
        char num_str[16];
        int num = start + i + 1;
        int pos = 0;
        if (num == 0) {
            num_str[pos++] = '0';
        } else {
            while (num > 0) {
                num_str[pos++] = '0' + (num % 10);
                num /= 10;
            }
        }
        
        // Inverser la chaîne
        for (int j = 0; j < pos / 2; j++) {
            char tmp = num_str[j];
            num_str[j] = num_str[pos - 1 - j];
            num_str[pos - 1 - j] = tmp;
        }
        num_str[pos] = '\0';
        
        print_string(num_str);
        print_string("  ");
        print_string(ctx->history.commands[i]);
        print_string("\n");
    }
    print_string("\n");
}

void cmd_env(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Variables d'Environnement ===\n", COLOR_CYAN);
    
    for (int i = 0; i < ctx->env_count; i++) {
        print_colored(ctx->env_vars[i].name, COLOR_YELLOW);
        print_string("=");
        print_string(ctx->env_vars[i].value);
        print_string("\n");
    }
    print_string("\n");
}

void cmd_echo(shell_context_t* ctx, char args[][128], int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        print_string(args[i]);
        if (i < arg_count - 1) print_string(" ");
    }
    print_string("\n");
}

void cmd_clear(shell_context_t* ctx, char args[][128], int arg_count) {
    // Séquence ANSI pour effacer l'écran
    print_string("\x1b[2J\x1b[H");
    
    // Banner de bienvenue moderne
    print_colored("═══════════════════════════════════════════════════════════\n", COLOR_CYAN);
    print_colored("    🤖 AI-OS v6.0 - Intelligence Artificielle Intégrée    \n", COLOR_BRIGHT);
    print_colored("═══════════════════════════════════════════════════════════\n", COLOR_CYAN);
    print_colored("💻 Shell Avancé", COLOR_GREEN);
    print_string(" | ");
    print_colored("🧠 IA Intelligente", COLOR_MAGENTA);
    print_string(" | ");
    print_colored("⚡ Haute Performance\n", COLOR_YELLOW);
    print_string("\n");
    print_info("Tapez 'help' pour voir toutes les commandes disponibles");
    print_info("Mode IA activé - Posez vos questions directement !");
    print_string("\n");
}

// === Builtins manquants / stubs ===
static void cmd_pwd(shell_context_t* ctx) {
    print_string(ctx->current_dir);
    print_string("\n");
}

static int is_absolute_path(const char* path) {
    return path && path[0] == '/';
}

static void normalize_path(char* out, const char* base, const char* add) {
    // Très simple: si add est absolu, le copier; sinon concaténer base + '/' + add
    if (is_absolute_path(add)) {
        strcpy(out, add);
    } else {
        strcpy(out, base);
        int len = strlen(out);
        if (len > 1 && out[len-1] == '/') out[len-1] = '\0';
        if (!(len == 1 && out[0] == '/')) strcat(out, "/");
        strcat(out, add);
    }
    // Retirer les doubles slash basiques
    char tmp[MAX_PATH_LENGTH];
    int j = 0;
    for (int i = 0; out[i] != '\0' && j < MAX_PATH_LENGTH-1; i++) {
        if (out[i] == '/' && out[i+1] == '/') continue;
        tmp[j++] = out[i];
    }
    tmp[j] = '\0';
    strcpy(out, tmp);
}

static void cmd_cd(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        // Aller à HOME
        const char* home = get_env_var(ctx, "HOME");
        if (!home) home = "/";
        strcpy(ctx->current_dir, home);
        return;
    }
    char newdir[MAX_PATH_LENGTH];
    normalize_path(newdir, ctx->current_dir, args[0]);
    // Pas de FS réel: accepter tout chemin "raisonnable"
    if (strlen(newdir) == 0) {
        print_error("cd: chemin invalide");
        return;
    }
    strcpy(ctx->current_dir, newdir);
}

static void cmd_cat(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_error("cat: fichier manquant");
        return;
    }
    // Pas d'accès FS direct en userspace: placeholder
    print_error("cat: non disponible en userspace (pas d'API FS) ");
}

static int is_builtin(const char* cmd) {
    return strcmp(cmd, "help")==0 || strcmp(cmd, "ls")==0 || strcmp(cmd, "dir")==0 ||
           strcmp(cmd, "ps")==0 || strcmp(cmd, "sysinfo")==0 || strcmp(cmd, "info")==0 ||
           strcmp(cmd, "mem")==0 || strcmp(cmd, "memory")==0 || strcmp(cmd, "history")==0 ||
           strcmp(cmd, "env")==0 || strcmp(cmd, "echo")==0 || strcmp(cmd, "clear")==0 ||
           strcmp(cmd, "cls")==0 || strcmp(cmd, "exit")==0 || strcmp(cmd, "quit")==0 ||
           strcmp(cmd, "ai")==0 || strcmp(cmd, "ai-mode")==0 || strcmp(cmd, "ai-help")==0 ||
           strcmp(cmd, "cd")==0 || strcmp(cmd, "pwd")==0 || strcmp(cmd, "cat")==0;
}

static void cmd_which(shell_context_t* ctx, const char* cmd) {
    if (is_builtin(cmd)) {
        print_string("builtin\n");
        return;
    }
    // Sans API FS: indiquer l'emplacement probable
    print_string("bin/");
    print_string(cmd);
    print_string(" (non vérifié)\n");
}

void cmd_exit(shell_context_t* ctx, char args[][128], int arg_count) {
    int exit_code = 0;
    
    if (arg_count > 0) {
        // Conversion simple string vers int
        int result = 0;
        char* str = args[0];
        for (int i = 0; str[i] != '\0'; i++) {
            if (str[i] >= '0' && str[i] <= '9') {
                result = result * 10 + (str[i] - '0');
            }
        }
        exit_code = result;
    }
    
    print_colored("\n🤖 Merci d'avoir utilisé AI-OS v6.0 !\n", COLOR_CYAN);
    print_colored("   Au revoir et à bientôt !\n\n", COLOR_YELLOW);
    
    exit_program(exit_code);
}

// ==============================================================================
// INTÉGRATION IA AVANCÉE
// ==============================================================================

void call_ai_assistant(shell_context_t* ctx, const char* query) {
    print_colored("\n🧠 [IA] ", COLOR_MAGENTA);
    
    // Préparer les arguments pour l'IA
    char* argv[3];
    argv[0] = "ai_assistant";
    argv[1] = (char*)query;
    argv[2] = NULL;
    
    // Exécuter le programme IA amélioré
    int result = exec("ai_assistant", argv);
    
    if (result != 0) {
        print_warning("Assistant IA temporairement indisponible");
        print_string("   Réponse automatique : Je travaille sur cette question...\n");
    }
    print_string("\n");
}

void cmd_ai(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_error("Usage: ai <votre question>");
        return;
    }
    
    // Reconstituer la question complète
    char full_query[MAX_COMMAND_LENGTH] = "";
    for (int i = 0; i < arg_count; i++) {
        strcat(full_query, args[i]);
        if (i < arg_count - 1) strcat(full_query, " ");
    }
    
    call_ai_assistant(ctx, full_query);
}

void cmd_ai_mode(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_string("Mode IA actuellement : ");
        print_colored(ctx->ai_mode ? "ACTIVÉ" : "DÉSACTIVÉ", 
                     ctx->ai_mode ? COLOR_GREEN : COLOR_RED);
        print_string("\n");
        return;
    }
    
    if (strcmp(args[0], "on") == 0) {
        ctx->ai_mode = 1;
        print_success("Mode IA activé - Vous pouvez maintenant poser des questions directement");
    } else if (strcmp(args[0], "off") == 0) {
        ctx->ai_mode = 0;
        print_success("Mode IA désactivé - Utilisez 'ai <question>' pour interroger l'IA");
    } else {
        print_error("Usage: ai-mode [on|off]");
    }
}

void cmd_ai_help(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Guide d'Utilisation de l'IA ===\n", COLOR_CYAN);
    
    print_colored("🧠 FONCTIONNALITÉS IA :\n", COLOR_MAGENTA);
    print_string("  • Réponses contextuelles intelligentes\n");
    print_string("  • Aide technique et suggestions\n");
    print_string("  • Analyse de commandes et diagnostics\n");
    print_string("  • Assistant personnel intégré\n\n");
    
    print_colored("💬 EXEMPLES DE QUESTIONS :\n", COLOR_YELLOW);
    print_string("  ai comment optimiser la mémoire ?\n");
    print_string("  ai explique-moi le multitâche\n");
    print_string("  ai que fait cette commande : ls -la\n");
    print_string("  ai résoudre erreur de compilation\n");
    print_string("  ai créer un script automatique\n\n");
    
    print_colored("⚙️ MODES D'UTILISATION :\n", COLOR_YELLOW);
    print_string("  1. Mode explicite : ai <question>\n");
    print_string("  2. Mode automatique : question directe (si activé)\n");
    print_string("  3. Mode intégré : aide contextuelle dans les commandes\n\n");
    
    print_colored("🎯 CONSEILS :\n", COLOR_GREEN);
    print_string("  • Soyez précis dans vos questions\n");
    print_string("  • Mentionnez le contexte si nécessaire\n");
    print_string("  • L'IA apprend de vos interactions\n\n");
}

// Test IA: lance l'IA avec une requete de sante et verifie le code retour
static void cmd_ai_test(shell_context_t* ctx) {
    print_colored("\n[AI-TEST] Starting healthcheck...\n", COLOR_CYAN);
    // Passer sans arguments: exec/ELF ne gère pas encore argv correctement
    char* argv[1];
    argv[0] = 0;
    int result = exec("bin/ai_assistant", argv);
    if (result == 0) {
        print_colored("[AI-TEST] OK\n", COLOR_GREEN);
    } else {
        print_colored("[AI-TEST] FAIL (ai_assistant not available)\n", COLOR_RED);
    }
    // Laisser l'ordonnanceur respirer avant de revenir au prompt
    yield();
}

// ==============================================================================
// GESTIONNAIRE DE COMMANDES PRINCIPAL
// ==============================================================================

int execute_builtin_command(shell_context_t* ctx, const char* command, 
                           char args[][128], int arg_count) {
    if (strcmp(command, "help") == 0) {
        cmd_help(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ls") == 0 || strcmp(command, "dir") == 0) {
        cmd_ls(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ps") == 0) {
        cmd_ps(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "sysinfo") == 0 || strcmp(command, "info") == 0) {
        cmd_sysinfo(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "mem") == 0 || strcmp(command, "memory") == 0) {
        cmd_mem(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "history") == 0) {
        cmd_history(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "env") == 0) {
        cmd_env(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "echo") == 0) {
        cmd_echo(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {
        cmd_clear(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "pwd") == 0) {
        cmd_pwd(ctx);
        return 1;
    } else if (strcmp(command, "cd") == 0) {
        cmd_cd(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "cat") == 0) {
        cmd_cat(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "which") == 0) {
        if (arg_count == 0) {
            print_error("which: commande manquante");
        } else {
            cmd_which(ctx, args[0]);
        }
        return 1;
    } else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
        cmd_exit(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai") == 0) {
        cmd_ai(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-mode") == 0) {
        cmd_ai_mode(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-help") == 0) {
        cmd_ai_help(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-test") == 0) {
        cmd_ai_test(ctx);
        return 1;
    }
    
    return 0; // Commande non trouvée
}

int is_question(const char* input) {
    // Détection simple de questions
    if (strstr(input, "?") != NULL) return 1;
    if (strncmp(input, "comment", 7) == 0) return 1;
    if (strncmp(input, "pourquoi", 8) == 0) return 1;
    if (strncmp(input, "qu'est-ce", 9) == 0) return 1;
    if (strncmp(input, "explain", 7) == 0) return 1;
    if (strncmp(input, "what", 4) == 0) return 1;
    if (strncmp(input, "how", 3) == 0) return 1;
    if (strncmp(input, "why", 3) == 0) return 1;
    
    return 0;
}

// ==============================================================================
// BOUCLE PRINCIPALE DU SHELL
// ==============================================================================

// Renvoie le nom de base du chemin (après le dernier '/')
static const char* get_basename(const char* path) {
    if (!path || path[0] == '\0') return "/";
    const char* end = path;
    // Aller à la fin
    while (*end) end++;
    // Sauter éventuels '/'
    while (end > path && *(end - 1) == '/') end--;
    if (end == path) return "/";
    // Chercher le dernier '/'
    const char* p = end;
    while (p > path && *(p - 1) != '/') p--;
    if (p == end) return "/"; // chemin composé uniquement de '/'
    return p;
}

void display_prompt(shell_context_t* ctx) {
    const char* folder = get_basename(ctx->current_dir);
    // Exemple: documents (-.-) :
    print_colored(folder, COLOR_BRIGHT);
    print_string(" (-.-) : ");
}

void handle_line(shell_context_t* ctx, char* input_buffer) {
    char command[128];
    char args[MAX_ARGS][128];
    int arg_count;

    // Ignorer les lignes vides
    if (strlen(input_buffer) == 0) {
        return;
    }
    
    // Ajouter à l'historique
    add_to_history(ctx, input_buffer);

    // Vérifier si c'est une question en mode IA
    if (ctx->ai_mode && is_question(input_buffer)) {
        call_ai_assistant(ctx, input_buffer);
        return;
    }

    // Parser la commande
    if (!parse_command(input_buffer, command, args, &arg_count)) {
        return;
    }

    // Exécuter la commande builtin
    if (execute_builtin_command(ctx, command, args, arg_count)) {
        return;
    }

    // Si pas de commande builtin, essayer d'exécuter un programme externe
    char* exec_args[MAX_ARGS + 2];
    exec_args[0] = command;
    for (int i = 0; i < arg_count; i++) {
        exec_args[i + 1] = args[i];
    }
    exec_args[arg_count + 1] = NULL;

    // Résolution simple PATH: essayer tel quel, puis bin/<cmd>
    int result = exec(command, exec_args);
    if (result != 0) {
        char alt[MAX_PATH_LENGTH];
        strcpy(alt, "bin/");
        strcat(alt, command);
        result = exec(alt, exec_args);
    }

    if (result != 0) {
        print_error("Commande non trouvée ou erreur d'exécution");
        print_string("   Tapez 'help' pour voir les commandes disponibles\n");
        
        // Suggestion IA si mode activé
        if (ctx->ai_mode) {
            print_colored("💡 Suggestion IA : ", COLOR_YELLOW);
            print_string("Voulez-vous que je vous aide avec cette commande ?\n");
        }
    }
}

void shell_main_loop(shell_context_t* ctx) {
    char buf[MAX_COMMAND_LENGTH];
    int idx;

    while (1) {
        display_prompt(ctx);
        idx = 0;
        buf[0] = '\0';

        for(;;){
            int c = sys_getchar();
            if (c == '\r' || c == '\n'){
                putc('\n');
                handle_line(ctx, buf);
                break; // sort de la boucle for(;;) pour ré-afficher le prompt
            }
            if (c == 0x08 || c == 127){ // Backspace
                if (idx > 0){
                    idx--;
                    buf[idx] = '\0';
                    backspace();
                }
                continue;
            }
            if (c == 0) {
                // Aucun caractere dispo: ceder le CPU pour laisser traiter les IRQ clavier
                yield();
                continue;
            }
            // N'accepter que les caractères ASCII imprimables (espace inclus)
            if (c >= 32 && c <= 126) {
                if (idx < (int)sizeof(buf) - 1){
                    buf[idx++] = (char)c;
                    buf[idx] = '\0';
                    putc((char)c);
                }
            }
        }
    }
}

// ==============================================================================
// POINT D'ENTRÉE PRINCIPAL
// ==============================================================================

void main() {
    shell_context_t shell_ctx;
    
    // Initialiser le contexte du shell
    init_shell_context(&shell_ctx);
    
    // Affichage de bienvenue moderne
    cmd_clear(&shell_ctx, NULL, 0);
    
    print_colored("🚀 Initialisation du Shell IA...", COLOR_CYAN);
    
    // Simulation d'initialisation progressive
    for (int i = 0; i < 3; i++) {
        for (volatile int j = 0; j < 10000000; j++); // Délai
        print_string(".");
    }
    print_string(" ");
    print_colored("TERMINÉ !\n\n", COLOR_GREEN);
    
    print_success("Shell AI-OS v6.0 prêt à l'utilisation");
    print_info("Mode IA activé - Intelligence artificielle intégrée");
    print_info("Tapez 'help' pour découvrir toutes les fonctionnalités");
    
    print_string("\n");
    
    // Démarrer la boucle principale
    shell_main_loop(&shell_ctx);
    
    // Ne devrait jamais être atteint
    exit_program(0);
}