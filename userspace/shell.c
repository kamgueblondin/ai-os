// shell.c - Shell Interactif Avancé pour AI-OS v6.0
// Shell utilisateur complet avec IA intégrée et fonctionnalités modernes

#include <stdint.h>
#include <stddef.h>
#include "ramfs.h"
#include "procsim.h"
#include "os_syscalls.h"
#include "os_vfs_service.h"
#include "os_ipc_deferred.h"

// ==============================================================================
// STRUCTURES ET DÉFINITIONS
// ==============================================================================

#define MAX_COMMAND_LENGTH 512
#define MAX_ARGS 32
#define MAX_HISTORY 50
#define MAX_PATH_LENGTH 256
#define MAX_ENV_VARS 32

/* Fournisseurs IA : la selection est persistante pendant la session du shell. */
#define AI_PROVIDER_LOCAL  0
#define AI_PROVIDER_OPENAI 1

/* Premier profil local cible : modele GGUF quantifie embarque sur le support de boot. */
#define AI_DEFAULT_MODEL "gpt2_124M.bin"

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
    int ai_provider;
    char ai_model[128];
    int debug_mode;
    int ai_query_count;
    int cmd_ticks;
    int last_rc;
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

int spawn(const char* path, char* argv[]) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(7), "b"(path), "c"(argv));
    return result;
}

void yield() {
    asm volatile("int $0x80" : : "a"(4));
}

int sys_listdir(const char* path, os_dirent_t* out, int max_n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_LISTDIR), "b"(path), "c"(out), "d"(max_n));
    return result;
}

int sys_readfile(const char* path, char* buf, int max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_READFILE), "b"(path), "c"(buf), "d"(max));
    return result;
}

int sys_getpid(void) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_GETPID));
    return result;
}

int sys_ps(os_proc_t* out, int max_n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_PS), "b"(out), "c"(max_n));
    return result;
}

int sys_kill_pid(int pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_KILL), "b"(pid));
    return result;
}

unsigned int sys_ticks(void) {
    unsigned int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TICKS));
    return result;
}

int sys_meminfo(os_meminfo_t* info) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_MEMINFO), "b"(info));
    return result;
}

int sys_task_metrics(int pid, os_task_metrics_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_METRICS), "b"(pid), "c"(out));
    return result;
}

int sys_task_set_priority(int pid, unsigned int priority) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_SET_PRIORITY), "b"(pid), "c"(priority));
    return result;
}

int sys_task_wait(int pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_WAIT), "b"(pid));
    return result;
}

int sys_task_set_name(int pid, const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_SET_NAME), "b"(pid), "c"(name));
    return result;
}

int sys_task_capacity(os_task_capacity_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CAPACITY), "b"(out));
    return result;
}

int sys_task_child_result(int pid, os_task_exit_result_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT), "b"(pid), "c"(out));
    return result;
}

int sys_task_child_result_list(os_task_exit_history_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT_LIST), "b"(out));
    return result;
}

int sys_task_child_result_ack(void) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT_ACK));
    return result;
}

int sys_task_child_result_observe(uint32_t expected, os_task_exit_history_observation_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT_OBSERVE), "b"(expected), "c"(out));
    return result;
}

int sys_task_child_result_find(int pid, os_task_exit_result_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT_FIND), "b"(pid), "c"(out));
    return result;
}

int sys_task_child_result_forget(int pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_TASK_CHILD_RESULT_FORGET), "b"(pid));
    return result;
}

int sys_mkdir(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_MKDIR), "b"(path));
    return result;
}

int sys_unlink(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_UNLINK), "b"(path));
    return result;
}

int sys_writefile(const char* path, const char* buf, int n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_WRITEFILE), "b"(path), "c"(buf), "d"(n));
    return result;
}

int sys_stat(const char* path, os_dirent_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_STAT), "b"(path), "c"(out));
    return result;
}

int sys_rename(const char* oldpath, const char* newpath) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_RENAME), "b"(oldpath), "c"(newpath));
    return result;
}

int sys_copy(const char* src, const char* dst) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_COPY), "b"(src), "c"(dst));
    return result;
}

int sys_append(const char* path, const char* buf, int n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_APPEND), "b"(path), "c"(buf), "d"(n));
    return result;
}

int sys_gpt2_generate(const char* prompt, char* out, int max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_GPT2_GENERATE), "b"(prompt), "c"(out), "d"(max));
    return result;
}

int sys_ipc_send(int target_pid, const os_ipc_payload_t* payload) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_IPC_SEND), "b"(target_pid), "c"(payload));
    return result;
}

int sys_ipc_receive(os_ipc_message_t* message) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_IPC_RECV), "b"(message));
    return result;
}

int sys_service_register(const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_REGISTER), "b"(name));
    return result;
}

int sys_service_lookup(const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_LOOKUP), "b"(name));
    return result;
}

int sys_service_status(const char* name, os_service_status_t* status) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_STATUS), "b"(name), "c"(status));
    return result;
}

int sys_service_grant(const char* name, int target_pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_GRANT), "b"(name), "c"(target_pid));
    return result;
}

int sys_service_notify(const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_NOTIFY), "b"(name));
    return result;
}

int sys_vfs_backend_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_BACKEND_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}

int sys_vfs_backend_write(const char* path, const char* data, uint32_t size) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_BACKEND_WRITE), "b"(path), "c"(data), "d"(size));
    return result;
}

int sys_vfs_overlay_unlink(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_UNLINK), "b"(path));
    return result;
}

int sys_vfs_overlay_rename(const char* oldpath, const char* newpath) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_RENAME), "b"(oldpath), "c"(newpath));
    return result;
}

static uint32_t vfs_request_counter = 0U;
static os_ipc_deferred_t ipc_deferred;

static uint32_t next_vfs_request_id(void) {
    vfs_request_counter++;
    if (vfs_request_counter == 0U) vfs_request_counter++;
    return vfs_request_counter;
}

static void print_fs_err(const char* cmd, int rc);

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

static int parse_int(const char* s) {
    int n = 0;
    int sign = 1;
    int i = 0;
    if (!s || s[0] == '\0') return 0;
    if (s[0] == '-') { sign = -1; i++; }
    for (; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') break;
        n = n * 10 + (s[i] - '0');
    }
    return sign * n;
}

static void print_int(int n) {
    char buf[16];
    int i = 0;
    unsigned int v;
    if (n < 0) {
        putc('-');
        v = (unsigned int)(-n);
    } else {
        v = (unsigned int)n;
    }
    if (v == 0) {
        putc('0');
        return;
    }
    while (v > 0 && i < 15) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i--) putc(buf[i]);
}

static char* find_char(const char* s, char c) {
    if (!s) return 0;
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return 0;
}

static void print_ramfs_err(const char* cmd, int err) {
    print_colored("[ERROR] ", COLOR_RED);
    print_string(cmd);
    print_string(": ");
    print_string(ramfs_strerror(err));
    print_string("\n");
}

static void resolve_arg(shell_context_t* ctx, const char* arg, char* out) {
    ramfs_resolve(ctx->current_dir, arg ? arg : ".", out, RAMFS_PATH_MAX);
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
    ctx->ai_provider = AI_PROVIDER_LOCAL;
    strcpy(ctx->ai_model, AI_DEFAULT_MODEL);
    ctx->debug_mode = 0;
    ctx->ai_query_count = 0;
    ctx->cmd_ticks = 0;
    ctx->last_rc = 0;
    os_ipc_deferred_init(&ipc_deferred);
    
    // Initialiser quelques variables d'environnement par défaut
    strcpy(ctx->env_vars[0].name, "PATH");
    strcpy(ctx->env_vars[0].value, "/bin:/usr/bin");
    strcpy(ctx->env_vars[1].name, "HOME");
    strcpy(ctx->env_vars[1].value, "/home/user");
    strcpy(ctx->env_vars[2].name, "SHELL");
    strcpy(ctx->env_vars[2].value, "ai-shell");
    strcpy(ctx->env_vars[3].name, "AI_OS_VERSION");
    strcpy(ctx->env_vars[3].value, "6.0");
    strcpy(ctx->env_vars[4].name, "USER");
    strcpy(ctx->env_vars[4].value, "root");
    ctx->env_count = 5;

    ramfs_init();
    procsim_init();
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
    print_string("  ls [path]          - Lister initrd + overlay noyau\n");
    print_string("  cat <file>         - Afficher un fichier (overlay puis initrd)\n");
    print_string("  stat <path>        - Type et taille (syscall SYS_STAT)\n");
    print_string("  test f|d|e <path>  - Tester fichier/dossier (SYS_STAT)\n");
    print_string("  [ f|d|e <path> ]   - Alias de test\n");
    print_string("  cd <path>          - Changer de répertoire\n");
    print_string("  pwd                - Afficher le répertoire courant\n");
    print_string("  mkdir <dir>        - Créer un répertoire (overlay noyau)\n");
    print_string("  rmdir <dir>        - Supprimer un répertoire vide\n");
    print_string("  cp <src> <dest>    - Copier fichier ou dossier overlay\n");
    print_string("  mv <src> <dest>    - Deplacer fichier ou dossier overlay\n");
    print_string("  rm <file>          - Supprimer un fichier\n");
    
    print_colored("\nCOMMANDES PROCESSUS :\n", COLOR_YELLOW);
    print_string("  ps                 - Afficher les processus\n");
    print_string("  spawn <prog>       - Lancer un programme (cede le CPU une fois)\n");
    print_string("  yield              - Ceder le CPU (SYS_YIELD, cooperatif)\n");
    print_string("  ipc-send <pid> <txt> - Envoyer un message IPC borne\n");
    print_string("  ipc-recv           - Lire un message IPC non bloquant\n");
    print_string("  service-publish <nom> - Publier un nom de service detenue par ce shell\n");
    print_string("  service-grant <nom> <pid> - Transferer un nom possede a une tache utilisateur\n");
    print_string("  service-find <nom> - Resoudre un service nomme\n");
    print_string("  service-status <nom> - Afficher la capacite IPC d'un service\n");
    print_string("  service-watch <nom> - S'abonner aux changements de proprietaire\n");
    print_string("  vfs-backend-probe <fichier> - Verifier le backend VFS reserve\n");
    print_string("  vfs-backend-write-probe <fichier> <texte> - Verifier l'ecriture backend reservee\n");
    print_string("  vfs-backend-remove-probe <fichier> - Verifier la suppression backend reservee\n");
    print_string("  vfs-backend-rename-probe <src> <dst> - Verifier le renommage backend reserve\n");
    print_string("  vfs-grant <pid>      - Demander au serveur VFS de transferer son nom\n");
    print_string("  vfs-backend-grant <pid> - Deleguer sans transfert un acces backend VFS\n");
    print_string("  vfs-backend-grant-read <pid> - Deleguer lecture backend VFS seulement\n");
    print_string("  vfs-backend-grant-mutate <pid> - Deleguer mutation backend VFS seulement\n");
    print_string("  vfs-backend-revoke <pid> - Retirer explicitement un acces backend VFS\n");
    print_string("  vfs-backend-status <pid> - Consulter le profil backend VFS d'un PID\n");
    print_string("  vfs-backend-list      - Lister les profils backend VFS actifs\n");
    print_string("  vfs-backend-observe <generation> - Observer un inventaire backend VFS\n");
    print_string("  vfs-read <fichier>   - Lire un fichier via le service VFS nomme\n");
    print_string("  vfs-stat <fichier>   - Lire les metadonnees via le service VFS nomme\n");
    print_string("  vfs-list <repertoire/> - Lister un repertoire monte via le service VFS\n");
    print_string("  vfs-mkdir <chemin> - Creer un repertoire via un montage overlay VFS\n");
    print_string("  vfs-rmdir <chemin> - Supprimer un repertoire vide via un montage overlay VFS\n");
    print_string("  vfs-list-page <repertoire/> <depart> - Lire une page VFS mediee\n");
    print_string("  vfs-list-observe <repertoire/> <depart> <generation> - Lire une page coherente\n");
    print_string("  vfs-stats            - Afficher les compteurs volatils du serveur VFS\n");
    print_string("  vfs-mount-add <prefixe/> <initrd|overlay> - Ajouter un alias VFS\n");
    print_string("  vfs-mount-remove <prefixe/> - Retirer un alias VFS dynamique\n");
    print_string("  vfs-write <chemin> <texte> - Ecrire via le montage VFS overlay/\n");
    print_string("  vfs-remove <chemin>  - Supprimer via le montage VFS overlay/\n");
    print_string("  vfs-rename <src> <dst> - Renommer via le montage VFS overlay/\n");
    print_string("  kill <pid>         - Terminer un processus\n");
    print_string("  jobs               - Afficher les tâches\n");
    print_string("  top                - Moniteur système\n");
    print_string("  getpid             - PID du shell (syscall SYS_GETPID)\n");
    
    print_colored("\nCOMMANDES SYSTÈME :\n", COLOR_YELLOW);
    print_string("  sysinfo            - Informations système\n");
    print_string("  task-metrics <pid> - Télémétrie d’une tâche\n");
    print_string("  task-priority <pid> <1|2|3> - Politique CPU locale\n");
    print_string("  task-name <pid> <nom> - Renommer soi ou un enfant direct\n");
    print_string("  task-capacity       - Capacité globale volatile des tâches\n");
    print_string("  child-result <pid> - Dernier résultat local d’un enfant terminé\n");
    print_string("  child-results      - Historique borné de résultats enfants\n");
    print_string("  child-results-clear - Acquitter l’historique enfant local\n");
    print_string("  child-results-observe <gen> - Observer l’historique à une génération\n");
    print_string("  child-result-any <pid> - Chercher un résultat enfant retenu\n");
    print_string("  child-results-forget <pid> - Acquitter un résultat enfant\n");
    print_string("  wait <pid>         - Attendre la sortie d’un enfant direct\n");
    print_string("  wait-result <pid>  - Attendre puis afficher le résultat enfant\n");
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
    print_string("  rc                 - Dernier code retour ($?)\n");
    
    print_colored("\nCOMMANDES INTELLIGENCE ARTIFICIELLE :\n", COLOR_YELLOW);
    print_string("  ai <question>      - Poser une question à l'IA\n");
    print_string("  ai-mode [on|off]   - Activer/désactiver le mode IA\n");
    print_string("  ai-help            - Aide sur l'utilisation de l'IA\n");
    print_string("  ai-stats           - Statistiques de l'IA\n");
    print_string("  ai-provider [nom]  - Choisir local ou openai\n");
    print_string("  ai-model [action]  - Lister ou choisir le modele local\n");
    print_string("  ai-runtime         - Etat du moteur IA et des prerequis\n");
    print_string("  net-status         - Etat reel de la pile reseau bare-metal\n");
    
    print_colored("\nCOMMANDES UTILITAIRES :\n", COLOR_YELLOW);
    print_string("  clear              - Effacer l'écran\n");
    print_string("  echo <text>        - Afficher du texte\n");
    print_string("  write <file> <txt> - Ecrire un fichier overlay (sans >)\n");
    print_string("  append <file> <txt> - Ajouter du texte (SYS_APPEND)\n");
    print_string("  touch <file>       - Creer un fichier overlay vide\n");
    print_string("  grep <pattern>     - Rechercher dans un texte\n");
    print_string("  wc <file>          - Compter lignes/mots/caractères\n");
    print_string("  sort <file>        - Trier les lignes (sort ok N fichier)\n");
    print_string("  head <file>        - Debut du fichier (head ok N fichier)\n");
    print_string("  tail <file>        - Fin du fichier (tail ok N fichier)\n");
    
    print_colored("\nCONTRÔLE :\n", COLOR_YELLOW);
    print_string("  exit [code]        - Quitter le shell\n");
    print_string("  logout             - Se déconnecter\n");
    print_string("  reboot             - Redémarrer le système\n");
    print_string("  shutdown           - Arrêter le système\n");
    
    print_colored("\nTIP: ls/cat/mkdir/rm/cp/mv/write/append parlent au noyau (initrd + overlay RAM).\n", COLOR_GREEN);
    print_colored("    Si le mode IA est activé, posez des questions sans 'ai'.\n\n", COLOR_GREEN);
}

void cmd_ls(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t kents[32];
    ramfs_dirent_t rents[RAMFS_MAX_LIST];
    int kn, rn;
    int shown = 0;

    if (arg_count > 0) resolve_arg(ctx, args[0], path);
    else resolve_arg(ctx, ".", path);

    print_colored("\n=== Initrd / VFS ===\n", COLOR_CYAN);
    print_string("chemin: ");
    print_string(path);
    print_string("\n");

    kn = sys_listdir(path, kents, 32);
    if (kn > 0) {
        for (int i = 0; i < kn; i++) {
            if (kents[i].flags == OS_DIRENT_DIR) {
                print_colored("drwxr-xr-x  ", COLOR_BLUE);
                print_colored(kents[i].name, COLOR_BLUE);
                print_string("/\n");
            } else {
                print_string("-rw-r--r--  ");
                print_int((int)kents[i].size);
                print_string("  ");
                print_string(kents[i].name);
                print_string("\n");
            }
            shown++;
        }
    }

    rn = ramfs_list(path, rents, RAMFS_MAX_LIST);
    if (rn > 0) {
        for (int i = 0; i < rn; i++) {
            int dup = 0;
            if (kn > 0) {
                for (int k = 0; k < kn; k++) {
                    if (strcmp(rents[i].name, kents[k].name) == 0) {
                        dup = 1;
                        break;
                    }
                }
            }
            if (dup) continue;
            if (rents[i].is_dir) {
                print_colored("drwxr-xr-x  ", COLOR_BLUE);
                print_colored(rents[i].name, COLOR_BLUE);
                print_string("/\n");
            } else {
                print_string("-rw-r--r--  ");
                print_int(rents[i].size);
                print_string("  ");
                print_string(rents[i].name);
                print_string("\n");
            }
            shown++;
        }
    }

    if (shown == 0 && kn < 0 && rn < 0) {
        print_error("ls: repertoire introuvable");
        return;
    }
    print_string("Total: ");
    print_int(shown);
    print_string(" elements\n\n");
}

static const char* proc_state_str(int st) {
    if (st == OS_TASK_RUNNING) return "R";
    if (st == OS_TASK_READY) return "S";
    if (st == OS_TASK_TERMINATED) return "Z";
    return "W";
}

void cmd_ps(shell_context_t* ctx, char args[][128], int arg_count) {
    os_proc_t procs[16];
    int n;
    (void)ctx; (void)args; (void)arg_count;
    n = sys_ps(procs, 16);
    print_colored("\n=== Processus (noyau) ===\n", COLOR_CYAN);
    print_colored("  PID  PPID  STAT  TYPE  COMMAND\n", COLOR_YELLOW);
    if (n < 0) n = 0;
    for (int i = 0; i < n; i++) {
        print_string("  ");
        print_int(procs[i].pid);
        print_string("    ");
        print_int(procs[i].parent_pid);
        print_string("    ");
        print_string(proc_state_str(procs[i].state));
        print_string("     ");
        print_string(procs[i].type == OS_TASK_USER ? "user  " : "kern  ");
        print_string(procs[i].name);
        print_string("\n");
    }
    print_string("Total: ");
    print_int(n);
    print_string("\n\n");
}

void cmd_task_metrics(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_metrics_t metrics;
    os_meminfo_t mem;
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: task-metrics <pid>");
        return;
    }
    rc = sys_task_metrics(pid, &metrics);
    if (rc != 0) {
        print_error("task-metrics: PID absent ou indisponible");
        return;
    }
    print_colored("\n=== Télémétrie tâche ===\n", COLOR_CYAN);
    print_string("PID : "); print_int(metrics.pid);
    print_string("\nParent : "); print_int(metrics.parent_pid);
    print_string("\nÉtat : "); print_string(proc_state_str(metrics.state));
    print_string("\nType : "); print_string(metrics.type == OS_TASK_USER ? "user" : "kernel");
    print_string("\nPriorité CPU : "); print_int((int)metrics.priority);
    print_string("\nÂge : "); print_int((int)metrics.age_ticks); print_string(" ticks");
    print_string("\nExécution cumulée : "); print_int((int)metrics.run_ticks); print_string(" ticks");
    print_string("\nCommutations : "); print_int((int)metrics.switch_count);
    print_string("\nEnfants directs : "); print_int((int)metrics.direct_children); print_string("\n");
    if (sys_meminfo(&mem) == 0) {
        print_string("PMM pages total/utilisées/libres : "); print_int((int)mem.total_pages);
        print_string("/"); print_int((int)mem.used_pages); print_string("/"); print_int((int)mem.free_pages); print_string("\n");
    }
    print_string("task-metrics ok "); print_int(metrics.pid); print_string(" "); print_int((int)metrics.priority); print_string(" "); print_int((int)metrics.run_ticks); print_string(" "); print_int((int)metrics.switch_count); print_string("\n");
}

void cmd_task_priority(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int priority;
    int rc;
    (void)ctx;
    if (arg_count != 2 || (pid = parse_int(args[0])) < 0 ||
        (priority = parse_int(args[1])) < (int)OS_TASK_PRIORITY_LOW ||
        priority > (int)OS_TASK_PRIORITY_HIGH) {
        print_error("Usage: task-priority <pid> <1|2|3>");
        return;
    }
    rc = sys_task_set_priority(pid, (unsigned int)priority);
    if (rc == OS_TASK_NOT_FOUND) {
        print_error("task-priority: PID absent");
        return;
    }
    if (rc == OS_TASK_BAD_PRIORITY) {
        print_error("task-priority: priorité hors plage");
        return;
    }
    if (rc == OS_TASK_CONTROL_DENIED) {
        print_error("task-priority: autorité limitée à soi ou enfant direct");
        return;
    }
    if (rc != 0) {
        print_error("task-priority: syscall indisponible");
        return;
    }
    print_string("task-priority ok "); print_int(pid); print_string(" "); print_int(priority); print_string("\n");
}

void cmd_task_wait(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: wait <pid>");
        return;
    }
    rc = sys_task_wait(pid);
    if (rc == OS_TASK_NOT_FOUND) {
        print_error("wait: PID absent");
        return;
    }
    if (rc == OS_TASK_NOT_CHILD) {
        print_error("wait: cible non enfant direct");
        return;
    }
    if (rc != 0) {
        print_error("wait: syscall indisponible");
        return;
    }
    print_string("wait ok "); print_int(pid); print_string("\n");
}

void cmd_wait_result(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_exit_result_t result;
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: wait-result <pid>");
        return;
    }
    rc = sys_task_wait(pid);
    if (rc == OS_TASK_NOT_FOUND) {
        rc = sys_task_child_result_find(pid, &result);
        if (rc == 0) {
            print_string("wait-result ok "); print_int(result.child_pid); print_string(" ");
            print_int(result.exit_code); print_string(" "); print_int((int)result.reason); print_string("\n");
            return;
        }
        print_error("wait-result: ni enfant actif ni résultat retenu");
        return;
    }
    if (rc == OS_TASK_NOT_CHILD) {
        print_error("wait-result: cible non enfant direct");
        return;
    }
    if (rc != 0) {
        print_error("wait-result: syscall d’attente indisponible");
        return;
    }
    rc = sys_task_child_result(pid, &result);
    if (rc != 0) {
        print_error("wait-result: résultat enfant indisponible");
        return;
    }
    print_string("wait-result ok "); print_int(result.child_pid); print_string(" ");
    print_int(result.exit_code); print_string(" "); print_int((int)result.reason); print_string("\n");
}

void cmd_task_name(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 2 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: task-name <pid> <nom>");
        return;
    }
    rc = sys_task_set_name(pid, args[1]);
    if (rc == OS_TASK_NOT_FOUND) {
        print_error("task-name: PID absent");
        return;
    }
    if (rc == OS_TASK_BAD_NAME) {
        print_error("task-name: nom invalide");
        return;
    }
    if (rc == OS_TASK_CONTROL_DENIED) {
        print_error("task-name: autorité limitée à soi ou enfant direct");
        return;
    }
    if (rc != 0) {
        print_error("task-name: syscall indisponible");
        return;
    }
    print_string("task-name ok "); print_int(pid); print_string(" "); print_string(args[1]); print_string("\n");
}

void cmd_task_capacity(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_capacity_t capacity;
    int rc;
    (void)ctx;
    if (arg_count != 0) {
        print_error("Usage: task-capacity");
        return;
    }
    rc = sys_task_capacity(&capacity);
    if (rc != 0) {
        print_error("task-capacity: syscall indisponible");
        return;
    }
    print_string("Tâches actives/capacité/disponibles : ");
    print_int((int)capacity.active); print_string("/");
    print_int((int)capacity.capacity); print_string("/");
    print_int((int)capacity.available); print_string("\n");
    print_string("task-capacity ok "); print_int((int)capacity.active); print_string(" ");
    print_int((int)capacity.capacity); print_string(" "); print_int((int)capacity.available); print_string("\n");
}

void cmd_child_result(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_exit_result_t result;
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: child-result <pid>");
        return;
    }
    rc = sys_task_child_result(pid, &result);
    if (rc == OS_TASK_NO_CHILD_RESULT) {
        print_error("child-result: aucun résultat local pour cet enfant");
        return;
    }
    if (rc == OS_TASK_NOT_FOUND) {
        print_error("child-result: parent indisponible");
        return;
    }
    if (rc != 0) {
        print_error("child-result: syscall indisponible");
        return;
    }
    print_string("Résultat enfant : PID "); print_int(result.child_pid);
    print_string(" code "); print_int(result.exit_code);
    print_string(" raison "); print_string(result.reason == OS_TASK_EVENT_EXITED ? "exited" : "killed");
    print_string(" tick "); print_int((int)result.finished_ticks); print_string("\n");
    print_string("child-result ok "); print_int(result.child_pid); print_string(" ");
    print_int(result.exit_code); print_string(" "); print_int((int)result.reason); print_string("\n");
}

void cmd_child_result_any(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_exit_result_t result;
    int pid;
    int rc;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: child-result-any <pid>");
        return;
    }
    rc = sys_task_child_result_find(pid, &result);
    if (rc == OS_TASK_NO_CHILD_RESULT) {
        print_error("child-result-any: aucun résultat retenu pour cet enfant");
        return;
    }
    if (rc != 0) {
        print_error("child-result-any: syscall indisponible");
        return;
    }
    print_string("child-result-any ok "); print_int(result.child_pid); print_string(" ");
    print_int(result.exit_code); print_string(" "); print_int((int)result.reason); print_string("\n");
}

void cmd_child_results_forget(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int generation;
    (void)ctx;
    if (arg_count != 1 || (pid = parse_int(args[0])) < 0) {
        print_error("Usage: child-results-forget <pid>");
        return;
    }
    generation = sys_task_child_result_forget(pid);
    if (generation == OS_TASK_NO_CHILD_RESULT) {
        print_error("child-results-forget: aucun résultat retenu pour cet enfant");
        return;
    }
    if (generation < 0) {
        print_error("child-results-forget: syscall indisponible");
        return;
    }
    print_string("child-results-forget ok "); print_int(pid); print_string(" ");
    print_int(generation); print_string("\n");
}

void cmd_child_results(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_exit_history_t history;
    uint32_t i;
    int rc;
    (void)ctx;
    if (arg_count != 0) {
        print_error("Usage: child-results");
        return;
    }
    rc = sys_task_child_result_list(&history);
    if (rc != 0) {
        print_error("child-results: syscall indisponible");
        return;
    }
    print_string("Historique résultats enfants : "); print_int((int)history.count); print_string("\n");
    for (i = 0U; i < history.count; i++) {
        print_string("  PID "); print_int(history.entries[i].child_pid);
        print_string(" code "); print_int(history.entries[i].exit_code);
        print_string(" raison "); print_string(history.entries[i].reason == OS_TASK_EVENT_EXITED ? "exited" : "killed");
        print_string(" tick "); print_int((int)history.entries[i].finished_ticks); print_string("\n");
        print_string("child-result-entry "); print_int(history.entries[i].child_pid); print_string(" ");
        print_int(history.entries[i].exit_code); print_string(" "); print_int((int)history.entries[i].reason); print_string("\n");
    }
    print_string("child-results ok "); print_int((int)history.count); print_string("\n");
}

void cmd_child_results_clear(shell_context_t* ctx, char args[][128], int arg_count) {
    int generation;
    (void)ctx;
    if (arg_count != 0) {
        print_error("Usage: child-results-clear");
        return;
    }
    generation = sys_task_child_result_ack();
    if (generation < 0) {
        print_error("child-results-clear: syscall indisponible");
        return;
    }
    print_string("child-results-clear ok "); print_int(generation); print_string("\n");
}

void cmd_child_results_observe(shell_context_t* ctx, char args[][128], int arg_count) {
    os_task_exit_history_observation_t observation;
    int expected;
    int rc;
    uint32_t i;
    (void)ctx;
    if (arg_count != 1 || (expected = parse_int(args[0])) < 0) {
        print_error("Usage: child-results-observe <generation>");
        return;
    }
    rc = sys_task_child_result_observe((uint32_t)expected, &observation);
    if (rc == OS_TASK_HISTORY_STALE) {
        print_string("child-results-observe stale "); print_int((int)observation.generation); print_string("\n");
        return;
    }
    if (rc != 0) {
        print_error("child-results-observe: syscall indisponible");
        return;
    }
    for (i = 0U; i < observation.history.count; i++) {
        print_string("child-result-entry "); print_int(observation.history.entries[i].child_pid); print_string(" ");
        print_int(observation.history.entries[i].exit_code); print_string(" "); print_int((int)observation.history.entries[i].reason); print_string("\n");
    }
    print_string("child-results-observe ok "); print_int((int)observation.generation); print_string(" ");
    print_int((int)observation.history.count); print_string("\n");
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
    {
        os_meminfo_t mi;
        if (sys_meminfo(&mi) == 0) {
            print_int((int)((mi.total_pages * 4) / 1024));
            print_string(" MB (PMM)\n");
        } else {
            print_string("inconnue\n");
        }
    }

    print_colored("Mémoire utilisée : ", COLOR_YELLOW);
    {
        os_meminfo_t mi;
        if (sys_meminfo(&mi) == 0) {
            print_int((int)((mi.used_pages * 4) / 1024));
            print_string(" MB\n");
        } else {
            print_string("inconnue\n");
        }
    }
    
    print_colored("Noyau : ", COLOR_YELLOW);
    print_string("AI-OS Kernel v6.0 (Multitâche préemptif)\n");
    
    print_colored("Shell : ", COLOR_YELLOW);
    print_string("AI-Shell v6.0 (IA intégrée)\n");
    
    print_colored("Système de fichiers : ", COLOR_YELLOW);
    print_string("Initrd TAR + overlay RAM\n");
    
    print_colored("Fonctionnalités : ", COLOR_YELLOW);
    print_string("PMM, VMM, Multitâche, IA, Ring 0/3\n");
    
    print_colored("Uptime : ", COLOR_YELLOW);
    {
        unsigned int ticks = sys_ticks();
        print_int((int)(ticks / 100));
        print_string(" s (PIT)\n\n");
    }
}

void cmd_mem(shell_context_t* ctx, char args[][128], int arg_count) {
    os_meminfo_t mi;
    (void)ctx; (void)args; (void)arg_count;
    print_colored("\n=== Utilisation Mémoire (PMM) ===\n", COLOR_CYAN);
    if (sys_meminfo(&mi) != 0) {
        print_error("mem: syscall indisponible");
        return;
    }
    print_colored("Pages physiques :\n", COLOR_YELLOW);
    print_string("  Total : ");
    print_int((int)mi.total_pages);
    print_string("\n  Utilisees : ");
    print_int((int)mi.used_pages);
    print_string("\n  Libres : ");
    print_int((int)mi.free_pages);
    print_string("\n  Taille page : 4 KB\n");
    print_string("mem ok ");
    print_int((int)mi.total_pages);
    print_string(" ");
    print_int((int)mi.used_pages);
    print_string(" ");
    print_int((int)mi.free_pages);
    print_string("\n");
}

void cmd_history(shell_context_t* ctx, char args[][128], int arg_count) {
    print_colored("\n=== Historique des Commandes ===\n", COLOR_CYAN);
    
    if (ctx->history.count == 0) {
        print_string("Aucune commande dans l'historique.\n");
        print_string("history ok 0\n");
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
    print_string("history ok ");
    print_int(ctx->history.count);
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
    print_string("env ok ");
    print_int(ctx->env_count);
    print_string("\n");
}

void cmd_echo(shell_context_t* ctx, char args[][128], int arg_count) {
    int redir = -1;
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], ">") == 0) {
            redir = i;
            break;
        }
    }
    if (redir >= 0) {
        char path[RAMFS_PATH_MAX];
        char buf[RAMFS_CONTENT_MAX];
        int pos = 0;
        int rc;
        if (redir + 1 >= arg_count) {
            print_error("echo: fichier manquant apres >");
            return;
        }
        for (int i = 0; i < redir; i++) {
            int j = 0;
            if (i > 0 && pos < RAMFS_CONTENT_MAX - 2) buf[pos++] = ' ';
            while (args[i][j] && pos < RAMFS_CONTENT_MAX - 2) buf[pos++] = args[i][j++];
        }
        buf[pos++] = '\n';
        buf[pos] = '\0';
        resolve_arg(ctx, args[redir + 1], path);
        rc = sys_writefile(path, buf, pos);
        if (rc < 0) print_fs_err("echo", rc);
        return;
    }
    for (int i = 0; i < arg_count; i++) {
        if (strcmp(args[i], "$?") == 0) print_int(ctx->last_rc);
        else print_string(args[i]);
        if (i < arg_count - 1) print_string(" ");
    }
    print_string("\n");
    print_string("echo ok\n");
}

void cmd_write(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    char buf[256];
    int pos = 0;
    int rc;
    if (arg_count == 0) {
        print_error("write: fichier manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    for (int i = 1; i < arg_count; i++) {
        int j = 0;
        if (i > 1 && pos < 254) buf[pos++] = ' ';
        while (args[i][j] && pos < 254) buf[pos++] = args[i][j++];
    }
    buf[pos++] = '\n';
    buf[pos] = '\0';
    rc = sys_writefile(path, buf, pos);
    if (rc < 0) {
        print_fs_err("write", rc);
        return;
    }
    print_string("write ok ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_append(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    char buf[256];
    int pos = 0;
    int rc;
    if (arg_count < 2) {
        print_error("append: fichier ou texte manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    for (int i = 1; i < arg_count; i++) {
        int j = 0;
        if (i > 1 && pos < 254) buf[pos++] = ' ';
        while (args[i][j] && pos < 254) buf[pos++] = args[i][j++];
    }
    buf[pos++] = '\n';
    buf[pos] = '\0';
    rc = sys_append(path, buf, pos);
    if (rc < 0) {
        print_fs_err("append", rc);
        return;
    }
    print_string("append ok ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_touch(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t st;
    int rc;
    if (arg_count == 0) {
        print_error("touch: fichier manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    if (sys_stat(path, &st) == 0) {
        if (st.flags == OS_DIRENT_DIR) {
            print_error("touch: est un repertoire");
            return;
        }
        print_string("touch ok ");
        print_string(args[0]);
        print_string("\n");
        return;
    }
    rc = sys_writefile(path, "", 0);
    if (rc < 0) {
        print_fs_err("touch", rc);
        return;
    }
    print_string("touch ok ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_stat(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t st;
    int size = 0;
    int is_dir = 0;
    if (arg_count == 0) {
        print_error("stat: chemin manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    if (sys_stat(path, &st) == 0) {
        is_dir = (st.flags == OS_DIRENT_DIR);
        size = (int)st.size;
    } else if (ramfs_is_dir(path)) {
        is_dir = 1;
        size = 0;
    } else if (ramfs_is_file(path)) {
        if (!ramfs_read(path, &size)) size = 0;
        is_dir = 0;
    } else {
        print_error("stat: introuvable");
        return;
    }
    print_string(is_dir ? "stat dir " : "stat file ");
    print_string(args[0]);
    print_string(" ");
    print_int(size);
    print_string("\n");
}

static int lookup_path_kind(shell_context_t* ctx, const char* arg, int* is_dir) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t st;
    resolve_arg(ctx, arg, path);
    if (sys_stat(path, &st) == 0) {
        *is_dir = (st.flags == OS_DIRENT_DIR);
        return 1;
    }
    if (ramfs_is_dir(path)) {
        *is_dir = 1;
        return 1;
    }
    if (ramfs_is_file(path)) {
        *is_dir = 0;
        return 1;
    }
    return 0;
}

static void cmd_test(shell_context_t* ctx, char args[][128], int arg_count) {
    int is_dir = 0;
    int found;
    int ok = 0;
    const char* flag;
    const char* name;
    if (arg_count < 2) {
        print_error("test: usage test f|d|e <chemin>");
        ctx->last_rc = 1;
        return;
    }
    flag = args[0];
    name = args[1];
    found = lookup_path_kind(ctx, name, &is_dir);
    if (strcmp(flag, "-f") == 0 || strcmp(flag, "f") == 0) ok = found && !is_dir;
    else if (strcmp(flag, "-d") == 0 || strcmp(flag, "d") == 0) ok = found && is_dir;
    else if (strcmp(flag, "-e") == 0 || strcmp(flag, "e") == 0) ok = found;
    else {
        print_error("test: flag inconnu");
        ctx->last_rc = 1;
        return;
    }
    if (ok) {
        if (strcmp(flag, "-f") == 0 || strcmp(flag, "f") == 0) print_string("test ok file ");
        else if (strcmp(flag, "-d") == 0 || strcmp(flag, "d") == 0) print_string("test ok dir ");
        else print_string("test ok ");
        print_string(name);
        print_string("\n");
        ctx->last_rc = 0;
    } else {
        print_string("test no ");
        print_string(name);
        print_string("\n");
        ctx->last_rc = 1;
    }
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

static void cmd_cd(shell_context_t* ctx, char args[][128], int arg_count) {
    char newdir[RAMFS_PATH_MAX];
    if (arg_count == 0) {
        const char* home = get_env_var(ctx, "HOME");
        if (!home) home = "/";
        ramfs_resolve("/", home, newdir, RAMFS_PATH_MAX);
    } else {
        ramfs_resolve(ctx->current_dir, args[0], newdir, RAMFS_PATH_MAX);
    }
    if (!ramfs_is_dir(newdir)) {
        os_dirent_t st;
        if (sys_stat(newdir, &st) != 0 || st.flags != OS_DIRENT_DIR) {
            print_error("cd: repertoire introuvable");
            return;
        }
    }
    strcpy(ctx->current_dir, newdir);
    print_string("cd ok ");
    if (arg_count == 0) print_string(ctx->current_dir);
    else print_string(args[0]);
    print_string("\n");
}

static void cmd_cat(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    char kbuf[1024];
    const char* data;
    int size = 0;
    int kn;
    if (arg_count == 0) {
        print_error("cat: fichier manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    kn = sys_readfile(path, kbuf, sizeof(kbuf));
    if (kn >= 0) {
        for (int i = 0; i < kn; i++) putc(kbuf[i]);
        if (kn == 0 || kbuf[kn - 1] != '\n') print_string("\n");
        return;
    }
    if (ramfs_is_dir(path)) {
        print_error("cat: est un repertoire");
        return;
    }
    data = ramfs_read(path, &size);
    if (!data) {
        print_error("cat: fichier introuvable");
        return;
    }
    for (int i = 0; i < size; i++) putc(data[i]);
    if (size == 0 || data[size - 1] != '\n') print_string("\n");
}

static int is_builtin(const char* cmd) {
    static const char* names[] = {
        "help", "ls", "dir", "ps", "task-metrics", "task-priority", "task-name", "task-capacity", "child-result", "child-result-any", "child-results", "child-results-clear", "child-results-observe", "child-results-forget", "wait", "wait-result", "sysinfo", "info", "mem", "memory",
        "history", "env", "echo", "write", "append", "touch", "clear", "cls", "exit", "quit",
        "ai", "ai-mode", "ai-help", "ai-test", "ai-stats", "ai-provider", "ai-model", "ai-runtime", "net-status",
        "cd", "pwd", "cat", "stat", "test", "[", "mkdir", "rmdir", "cp", "mv", "rm",
        "kill", "spawn", "yield", "ipc-send", "ipc-recv", "service-publish", "service-grant", "service-find", "service-status", "service-watch", "vfs-backend-probe", "vfs-backend-write-probe", "vfs-backend-remove-probe", "vfs-backend-rename-probe", "vfs-grant", "vfs-read", "vfs-stat", "vfs-stats", "vfs-mount-add", "vfs-mount-remove", "vfs-write", "vfs-remove", "vfs-rename", "vfs-mkdir", "vfs-rmdir", "jobs", "top", "getpid", "uptime", "date", "whoami",
        "alias", "unalias", "export", "which", "rc",
        "grep", "wc", "sort", "head", "tail",
        "logout", "reboot", "shutdown",
        "aistats", "aimode", "aihelp", "aitest",
        0
    };
    for (int i = 0; names[i]; i++) {
        if (strcmp(cmd, names[i]) == 0) return 1;
    }
    return 0;
}

static void cmd_which(shell_context_t* ctx, const char* cmd) {
    (void)ctx;
    if (is_builtin(cmd)) {
        print_string("which ok builtin ");
        print_string(cmd);
        print_string("\n");
        return;
    }
    print_string("which ok bin/");
    print_string(cmd);
    print_string("\n");
}

static void print_fs_err(const char* cmd, int rc) {
    print_string(cmd);
    print_string(": ");
    if (rc == -2) print_string("existe deja\n");
    else if (rc == -3) print_string("parent invalide\n");
    else if (rc == -4) print_string("est un repertoire\n");
    else if (rc == -5) print_string("repertoire non vide\n");
    else if (rc == -6) print_string("plus de place\n");
    else if (rc == -8) print_string("protege (initrd)\n");
    else print_string("echec\n");
}

static void cmd_mkdir(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    int rc;
    if (arg_count == 0) {
        print_error("mkdir: repertoire manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    rc = sys_mkdir(path);
    if (rc != 0) {
        print_fs_err("mkdir", rc);
        return;
    }
    print_string("mkdir ok ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_rmdir(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t st;
    int rc;
    if (arg_count == 0) {
        print_error("rmdir: repertoire manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    if (sys_stat(path, &st) == 0) {
        if (st.flags != OS_DIRENT_DIR) {
            print_error("rmdir: n'est pas un repertoire");
            return;
        }
        rc = sys_unlink(path);
        if (rc != 0) {
            print_fs_err("rmdir", rc);
            return;
        }
        print_string("rmdir ok ");
        print_string(args[0]);
        print_string("\n");
        return;
    }
    rc = ramfs_rmdir(path);
    if (rc != RAMFS_OK) print_ramfs_err("rmdir", rc);
}

static void cmd_rm(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    os_dirent_t st;
    int rc;
    if (arg_count == 0) {
        print_error("rm: fichier manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    if (sys_stat(path, &st) == 0) {
        if (st.flags == OS_DIRENT_DIR) {
            print_error("rm: est un repertoire");
            return;
        }
        rc = sys_unlink(path);
        if (rc != 0) {
            print_fs_err("rm", rc);
            return;
        }
        print_string("rm ok ");
        print_string(args[0]);
        print_string("\n");
        return;
    }
    rc = ramfs_rm(path);
    if (rc != RAMFS_OK) print_ramfs_err("rm", rc);
}

static const char* fs_basename(const char* path) {
    const char* b = path ? path : "";
    int i = 0;
    while (path && path[i]) {
        if (path[i] == '/') b = path + i + 1;
        i++;
    }
    return (b && b[0]) ? b : "/";
}

static void fs_join(char* out, int max, const char* dir, const char* name) {
    int i = 0;
    int j = 0;
    if (!dir) dir = "/";
    if (!name) name = "";
    while (dir[i] && i < max - 2) {
        out[i] = dir[i];
        i++;
    }
    if (i > 0 && out[i - 1] != '/') out[i++] = '/';
    while (name[j] && i < max - 1) out[i++] = name[j++];
    out[i] = '\0';
}

static int kernel_copy_file_to(const char* src, const char* dest) {
    os_dirent_t st;
    char buf[256];
    int n;
    int w;
    if (sys_stat(src, &st) != 0) return -1;
    if (st.flags == OS_DIRENT_DIR) return -4;
    n = sys_readfile(src, buf, (int)sizeof(buf));
    if (n < 0) return n;
    if (sys_stat(dest, &st) == 0 && st.flags == OS_DIRENT_DIR) return -4;
    w = sys_writefile(dest, buf, n);
    if (w < 0) return w;
    return 0;
}

static void cmd_cp(shell_context_t* ctx, char args[][128], int arg_count) {
    char src[RAMFS_PATH_MAX];
    char dst[RAMFS_PATH_MAX];
    os_dirent_t st;
    int src_dir;
    int rc;
    if (arg_count < 2) {
        print_error("cp: usage cp <src> <dest>");
        return;
    }
    resolve_arg(ctx, args[0], src);
    resolve_arg(ctx, args[1], dst);
    if (sys_stat(src, &st) == 0) {
        src_dir = (st.flags == OS_DIRENT_DIR);
        if (sys_stat(dst, &st) == 0 && st.flags == OS_DIRENT_DIR) {
            char joined[RAMFS_PATH_MAX];
            fs_join(joined, RAMFS_PATH_MAX, dst, fs_basename(src));
            strcpy(dst, joined);
        }
        if (src_dir) rc = sys_copy(src, dst);
        else rc = kernel_copy_file_to(src, dst);
        if (rc != 0) {
            print_fs_err("cp", rc);
            return;
        }
        print_string("cp ok ");
        print_string(fs_basename(dst));
        print_string("\n");
        return;
    }
    rc = ramfs_cp(src, dst);
    if (rc != RAMFS_OK) print_ramfs_err("cp", rc);
}

static void cmd_mv(shell_context_t* ctx, char args[][128], int arg_count) {
    char src[RAMFS_PATH_MAX];
    char dst[RAMFS_PATH_MAX];
    os_dirent_t st;
    int rc;
    if (arg_count < 2) {
        print_error("mv: usage mv <src> <dest>");
        return;
    }
    resolve_arg(ctx, args[0], src);
    resolve_arg(ctx, args[1], dst);
    if (sys_stat(src, &st) == 0) {
        if (sys_stat(dst, &st) == 0 && st.flags == OS_DIRENT_DIR) {
            char joined[RAMFS_PATH_MAX];
            fs_join(joined, RAMFS_PATH_MAX, dst, fs_basename(src));
            strcpy(dst, joined);
        }
        rc = sys_rename(src, dst);
        if (rc != 0) {
            print_fs_err("mv", rc);
            return;
        }
        print_string("mv ok ");
        print_string(fs_basename(dst));
        print_string("\n");
        return;
    }
    rc = ramfs_mv(src, dst);
    if (rc != RAMFS_OK) print_ramfs_err("mv", rc);
}

static void cmd_kill(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int rc;
    (void)ctx;
    if (arg_count == 0) {
        print_error("kill: pid manquant");
        return;
    }
    pid = parse_int(args[0]);
    rc = sys_kill_pid(pid);
    if (rc == -2) print_error("kill: processus protege (kernel)");
    else if (rc == -3) print_error("kill: impossible de tuer le shell courant (exit)");
    else if (rc != 0) print_error("kill: pid introuvable");
    else {
        print_string("Processus ");
        print_int(pid);
        print_string(" termine\n");
    }
}

static void cmd_spawn(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    (void)ctx;
    if (arg_count == 0) {
        print_error("spawn: programme manquant");
        return;
    }
    pid = spawn(args[0], 0);
    if (pid == OS_TASK_CHILD_LIMIT) {
        print_error("spawn: capacité de quatre enfants atteinte");
        return;
    }
    if (pid < 0) {
        char alt[80];
        int i = 0;
        alt[0] = 'b'; alt[1] = 'i'; alt[2] = 'n'; alt[3] = '/';
        while (args[0][i] && i < 70) {
            alt[4 + i] = args[0][i];
            i++;
        }
        alt[4 + i] = '\0';
        pid = spawn(alt, 0);
    }
    if (pid == OS_TASK_CHILD_LIMIT) {
        print_error("spawn: capacité de quatre enfants atteinte");
        return;
    }
    if (pid < 0) {
        print_error("spawn: programme introuvable");
        return;
    }
    print_string("spawn ok pid ");
    print_int(pid);
    print_string(" ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_ipc_send(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t payload;
    int pid;
    int rc;
    uint32_t i = 0U;
    if (arg_count != 2) {
        print_error("Usage: ipc-send <pid> <texte_sans_espace>");
        return;
    }
    pid = parse_int(args[0]);
    if (pid <= 0) {
        print_error("ipc-send: pid invalide");
        return;
    }
    payload.type = 0U;
    payload.request_id = 0U;
    while (args[1][i] != '\0' && i < OS_IPC_MAX_DATA) {
        payload.data[i] = (uint8_t)args[1][i];
        i++;
    }
    if (args[1][i] != '\0') {
        print_error("ipc-send: texte trop long");
        return;
    }
    payload.size = i;
    while (i < OS_IPC_MAX_DATA) payload.data[i++] = 0U;
    rc = sys_ipc_send(pid, &payload);
    ctx->last_rc = rc;
    if (rc == OS_IPC_SERVICE_FULL) print_error("ipc-send: capacite du service atteinte");
    else if (rc == OS_IPC_FULL) print_error("ipc-send: boite aux lettres pleine");
    else if (rc == OS_IPC_BAD_TARGET) print_error("ipc-send: cible utilisateur introuvable");
    else if (rc != 0) print_error("ipc-send: message invalide");
    else {
        print_string("ipc-send ok ");
        print_int(pid);
        print_string(" ");
        print_int((int)payload.size);
        print_string("\n");
    }
}

static void cmd_ipc_recv(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_message_t message;
    os_service_event_t event;
    os_task_event_t task_event;
    int rc;
    uint32_t i;
    (void)args;
    (void)arg_count;
    rc = os_ipc_deferred_take(&ipc_deferred, &message);
    if (rc == OS_IPC_EMPTY) rc = sys_ipc_receive(&message);
    ctx->last_rc = rc;
    if (rc == OS_IPC_EMPTY) {
        print_string("ipc-recv empty\n");
        return;
    }
    if (rc != 0) {
        print_error("ipc-recv: erreur");
        return;
    }
    if (os_service_parse_event(&message, &event) == 0) {
        print_string("service-event ");
        print_string(event.name);
        print_string(" old ");
        print_int(event.old_owner_pid);
        print_string(" new ");
        print_int(event.new_owner_pid);
        print_string(" reason ");
        print_int((int)event.reason);
        print_string("\n");
        return;
    }
    if (os_task_parse_event(&message, &task_event) == 0) {
        print_string("task-event child ");
        print_int(task_event.child_pid);
        print_string(" reason ");
        print_string(task_event.reason == OS_TASK_EVENT_EXITED ? "exited" : "killed");
        print_string("\n");
        return;
    }
    print_string("ipc-recv from ");
    print_int(message.sender_pid);
    print_string(" type ");
    print_int((int)message.type);
    print_string(" data ");
    for (i = 0U; i < message.size; i++) putc((char)message.data[i]);
    print_string("\n");
}

static void cmd_service_publish(shell_context_t* ctx, char args[][128], int arg_count) {
    int rc;
    if (arg_count != 1) {
        print_error("Usage: service-publish <nom>");
        return;
    }
    rc = sys_service_register(args[0]);
    ctx->last_rc = rc;
    if (rc == 0) {
        print_string("service-publish ok ");
        print_string(args[0]);
        print_string("\n");
    } else if (rc == OS_SERVICE_TAKEN) {
        print_error("service-publish: nom deja reserve");
    } else {
        print_error("service-publish: nom invalide ou registre plein");
    }
}

static void cmd_service_grant(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    int rc;
    if (arg_count != 2) {
        print_error("Usage: service-grant <nom> <pid>");
        return;
    }
    pid = parse_int(args[1]);
    if (pid <= 0) {
        print_error("service-grant: pid invalide");
        return;
    }
    rc = sys_service_grant(args[0], pid);
    ctx->last_rc = rc;
    if (rc == 0) {
        print_string("service-grant ok ");
        print_string(args[0]);
        print_string(" ");
        print_int(pid);
        print_string("\n");
    } else if (rc == OS_SERVICE_NOT_OWNER) {
        print_error("service-grant: nom non detenue par ce shell");
    } else if (rc == OS_SERVICE_BAD_GRANTEE) {
        print_error("service-grant: beneficiaire utilisateur introuvable");
    } else {
        print_error("service-grant: nom invalide ou indisponible");
    }
}

static void cmd_service_find(shell_context_t* ctx, char args[][128], int arg_count) {
    int pid;
    if (arg_count != 1) {
        print_error("Usage: service-find <nom>");
        return;
    }
    pid = sys_service_lookup(args[0]);
    ctx->last_rc = pid < 0 ? pid : 0;
    if (pid > 0) {
        print_string("service-find ok ");
        print_string(args[0]);
        print_string(" ");
        print_int(pid);
        print_string("\n");
    } else {
        print_error("service-find: service indisponible");
    }
}

static void cmd_service_status(shell_context_t* ctx, char args[][128], int arg_count) {
    os_service_status_t status;
    int rc;
    if (arg_count != 1) {
        print_error("Usage: service-status <nom>");
        return;
    }
    rc = sys_service_status(args[0], &status);
    ctx->last_rc = rc;
    if (rc == 0) {
        print_string("service-status ok ");
        print_string(args[0]);
        print_string(" pid ");
        print_int(status.owner_pid);
        print_string(" queued ");
        print_int((int)status.queued_messages);
        print_string(" client-capacity ");
        print_int((int)status.client_capacity);
        print_string(" endpoint-capacity ");
        print_int((int)status.endpoint_capacity);
        print_string("\n");
    } else if (rc == OS_SERVICE_NOT_FOUND) {
        print_error("service-status: service indisponible");
    } else {
        print_error("service-status: requete invalide");
    }
}

static void cmd_service_watch(shell_context_t* ctx, char args[][128], int arg_count) {
    int rc;
    if (arg_count != 1) {
        print_error("Usage: service-watch <nom>");
        return;
    }
    rc = sys_service_notify(args[0]);
    ctx->last_rc = rc;
    if (rc == 0) {
        print_string("service-watch ok ");
        print_string(args[0]);
        print_string("\n");
    } else if (rc == OS_SERVICE_WATCH_FULL) {
        print_error("service-watch: limite d'abonnements atteinte");
    } else {
        print_error("service-watch: nom invalide");
    }
}

static void cmd_vfs_backend_probe(shell_context_t* ctx, char args[][128], int arg_count) {
    char data[OS_VFS_READ_MAX];
    int rc;
    if (arg_count != 1) {
        print_error("Usage: vfs-backend-probe <fichier>");
        return;
    }
    rc = sys_vfs_backend_read(args[0], data, OS_VFS_READ_MAX);
    ctx->last_rc = rc;
    if (rc == OS_VFS_BACKEND_DENIED) {
        print_string("vfs-backend-probe denied\n");
    } else {
        print_error("vfs-backend-probe: acces inattendu ou erreur backend");
    }
}

static void cmd_vfs_backend_write_probe(shell_context_t* ctx, char args[][128], int arg_count) {
    uint32_t size = 0U;
    int rc;
    if (arg_count != 2) {
        print_error("Usage: vfs-backend-write-probe <fichier> <texte>");
        return;
    }
    while (args[1][size] != '\0') size++;
    rc = sys_vfs_backend_write(args[0], args[1], size);
    ctx->last_rc = rc;
    if (rc == OS_VFS_BACKEND_DENIED) {
        print_string("vfs-backend-write-probe denied\n");
    } else if (rc == 0) {
        print_string("vfs-backend-write-probe unexpectedly allowed\n");
    } else {
        print_error("vfs-backend-write-probe: erreur backend");
    }
}

static void cmd_vfs_backend_remove_probe(shell_context_t* ctx, char args[][128], int arg_count) {
    int rc;
    if (arg_count != 1) {
        print_error("Usage: vfs-backend-remove-probe <fichier>");
        return;
    }
    rc = sys_vfs_overlay_unlink(args[0]);
    ctx->last_rc = rc;
    if (rc == OS_VFS_BACKEND_DENIED) {
        print_string("vfs-backend-remove-probe denied\n");
    } else if (rc == 0) {
        print_string("vfs-backend-remove-probe unexpectedly allowed\n");
    } else {
        print_error("vfs-backend-remove-probe: erreur backend");
    }
}

static void cmd_vfs_backend_rename_probe(shell_context_t* ctx, char args[][128], int arg_count) {
    int rc;
    if (arg_count != 2) {
        print_error("Usage: vfs-backend-rename-probe <src> <dst>");
        return;
    }
    rc = sys_vfs_overlay_rename(args[0], args[1]);
    ctx->last_rc = rc;
    if (rc == OS_VFS_BACKEND_DENIED) {
        print_string("vfs-backend-rename-probe denied\n");
    } else if (rc == 0) {
        print_string("vfs-backend-rename-probe unexpectedly allowed\n");
    } else {
        print_error("vfs-backend-rename-probe: erreur backend");
    }
}

static void cmd_vfs_grant(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    int pid;
    int target_pid;
    int rc;
    if (arg_count != 1) {
        print_error("Usage: vfs-grant <pid>");
        return;
    }
    target_pid = parse_int(args[0]);
    if (target_pid <= 0) {
        print_error("vfs-grant: pid invalide");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-grant: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    rc = os_vfs_make_grant_request(&request, target_pid);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    ctx->last_rc = rc;
    if (rc == 0) {
        print_string("vfs-grant sent ");
        print_int(target_pid);
        print_string(" via ");
        print_int(pid);
        print_string("\n");
    } else {
        print_error("vfs-grant: demande refusee");
    }
}

static void cmd_vfs_backend_grant(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, target_pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1 || (target_pid = parse_int(args[0])) <= 0) { print_error("Usage: vfs-backend-grant <pid>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-grant: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_grant_request(&request, target_pid, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-grant: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_GRANT_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_grant_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_GRANT_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_grant_reply(&message, &status, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || status != 0) { print_error("vfs-backend-grant: delegation refusee"); ctx->last_rc = rc != 0 ? rc : status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-grant ok request "); print_int((int)request_id); print_string("\n");
}

static void cmd_vfs_backend_grant_read(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, target_pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1 || (target_pid = parse_int(args[0])) <= 0) { print_error("Usage: vfs-backend-grant-read <pid>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-grant-read: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_grant_scoped_request(&request, target_pid, OS_VFS_BACKEND_RIGHT_READ, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-grant-read: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_GRANT_SCOPED_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_grant_scoped_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_GRANT_SCOPED_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_grant_scoped_reply(&message, &status, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || status != 0) { print_error("vfs-backend-grant-read: delegation refusee"); ctx->last_rc = rc != 0 ? rc : status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-grant-read ok request "); print_int((int)request_id); print_string("\n");
}

static void cmd_vfs_backend_grant_mutate(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, target_pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1 || (target_pid = parse_int(args[0])) <= 0) { print_error("Usage: vfs-backend-grant-mutate <pid>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-grant-mutate: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_grant_scoped_request(&request, target_pid, OS_VFS_BACKEND_RIGHT_MUTATE, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-grant-mutate: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_GRANT_SCOPED_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_grant_scoped_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_GRANT_SCOPED_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_grant_scoped_reply(&message, &status, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || status != 0) { print_error("vfs-backend-grant-mutate: delegation refusee"); ctx->last_rc = rc != 0 ? rc : status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-grant-mutate ok request "); print_int((int)request_id); print_string("\n");
}

static void cmd_vfs_backend_revoke(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, target_pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1 || (target_pid = parse_int(args[0])) <= 0) { print_error("Usage: vfs-backend-revoke <pid>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-revoke: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_revoke_request(&request, target_pid, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-revoke: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_REVOKE_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_revoke_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_REVOKE_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_revoke_reply(&message, &status, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || status != 0) { print_error("vfs-backend-revoke: revocation refusee"); ctx->last_rc = rc != 0 ? rc : status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-revoke ok request "); print_int((int)request_id); print_string("\n");
}

static void cmd_vfs_backend_status(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, target_pid, rc, status, attempts; uint32_t request_id, rights;
    if (arg_count != 1 || (target_pid = parse_int(args[0])) <= 0) { print_error("Usage: vfs-backend-status <pid>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-status: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_status_request(&request, target_pid, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-status: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_STATUS_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_status_reply(&message, &status, &rights, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_STATUS_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_status_reply(&message, &status, &rights, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || status != 0) { print_error("vfs-backend-status: capacite absente ou refusee"); ctx->last_rc = rc != 0 ? rc : status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-status ok rights ");
    if (rights == OS_VFS_BACKEND_RIGHT_READ) print_string("read");
    else if (rights == OS_VFS_BACKEND_RIGHT_MUTATE) print_string("mutate");
    else if (rights == OS_VFS_BACKEND_RIGHT_ALL) print_string("full");
    else { print_error("vfs-backend-status: masque invalide"); ctx->last_rc = OS_VFS_STATUS_INVALID; return; }
    print_string(" request "); print_int((int)request_id); print_string("\n");
}

static void cmd_vfs_backend_list(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message; os_vfs_backend_list_reply_t reply;
    int pid, rc, attempts; uint32_t request_id, i;
    if (arg_count != 0) { print_error("Usage: vfs-backend-list"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-list: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_list_request(&request, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-list: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_LIST_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_list_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_LIST_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_list_reply(&message, &reply, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0 || reply.status != 0) { print_error("vfs-backend-list: consultation refusee"); ctx->last_rc = rc != 0 ? rc : reply.status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-list ok count "); print_int((int)reply.count); print_string(" request "); print_int((int)request_id); print_string("\n");
    for (i = 0U; i < reply.count; i++) {
        print_string("vfs-backend-list pid "); print_int(reply.entries[i].pid); print_string(" rights ");
        if (reply.entries[i].rights == OS_VFS_BACKEND_RIGHT_READ) print_string("read");
        else if (reply.entries[i].rights == OS_VFS_BACKEND_RIGHT_MUTATE) print_string("mutate");
        else if (reply.entries[i].rights == OS_VFS_BACKEND_RIGHT_ALL) print_string("full");
        else { print_error("vfs-backend-list: masque invalide"); ctx->last_rc = OS_VFS_STATUS_INVALID; return; }
        print_string("\n");
    }
}

static void cmd_vfs_backend_observe(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message; os_vfs_backend_observe_reply_t reply;
    int pid, rc, attempts, expected; uint32_t request_id;
    if (arg_count != 1 || (expected = parse_int(args[0])) < 0) { print_error("Usage: vfs-backend-observe <generation>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-backend-observe: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_backend_observe_request(&request, (uint32_t)expected, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-backend-observe: demande refusee"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_BACKEND_OBSERVE_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_backend_observe_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) { int saved; yield(); rc = sys_ipc_receive(&message); if (rc == 0) { if (message.type == OS_IPC_VFS_BACKEND_OBSERVE_REPLY && message.request_id == request_id && message.sender_pid == pid) rc = os_vfs_parse_backend_observe_reply(&message, &reply, request_id); else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; } } }
    if (rc != 0) { print_error("vfs-backend-observe: reponse invalide"); ctx->last_rc = rc; return; }
    if (reply.status == OS_SERVICE_STALE) { ctx->last_rc = reply.status; print_string("vfs-backend-observe stale generation "); print_int((int)reply.generation); print_string("\n"); return; }
    if (reply.status != 0) { print_error("vfs-backend-observe: consultation refusee"); ctx->last_rc = reply.status; return; }
    ctx->last_rc = 0; print_string("vfs-backend-observe ok generation "); print_int((int)reply.generation); print_string(" count "); print_int((int)reply.count); print_string("\n");
}

static void cmd_vfs_write(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_write_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    uint32_t size = 0U;
    if (arg_count != 2) {
        print_error("Usage: vfs-write <chemin> <texte>");
        return;
    }
    while (args[1][size] != '\0') size++;
    if (size > OS_VFS_WRITE_MAX) {
        print_error("vfs-write: texte trop long");
        ctx->last_rc = OS_VFS_STATUS_INVALID;
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-write: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_write_request(&request, args[0], (const uint8_t*)args[1],
                                   size, request_id);
    if (rc != 0) {
        print_error("vfs-write: chemin invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-write: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_WRITE_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_write_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_WRITE_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_write_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-write: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-write: chemin hors montage ecriture");
        } else {
            print_error("vfs-write: ecriture refusee");
        }
        return;
    }
    print_string("vfs-write ok request ");
    print_int((int)request_id);
    print_string("\n");
}

static void cmd_vfs_remove(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_remove_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    if (arg_count != 1) {
        print_error("Usage: vfs-remove <chemin>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-remove: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_remove_request(&request, args[0], request_id);
    if (rc != 0) {
        print_error("vfs-remove: chemin invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-remove: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_REMOVE_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_remove_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_REMOVE_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_remove_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-remove: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-remove: chemin hors montage ecriture");
        } else {
            print_error("vfs-remove: suppression refusee ou fichier absent");
        }
        return;
    }
    print_string("vfs-remove ok request ");
    print_int((int)request_id);
    print_string("\n");
}

static void cmd_vfs_rename(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_rename_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    if (arg_count != 2) {
        print_error("Usage: vfs-rename <src> <dst>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-rename: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_rename_request(&request, args[0], args[1], request_id);
    if (rc != 0) {
        print_error("vfs-rename: chemin invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-rename: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_RENAME_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_rename_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_RENAME_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_rename_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-rename: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-rename: chemins hors montage ecriture");
        } else {
            print_error("vfs-rename: renommage refuse ou fichier absent");
        }
        return;
    }
    print_string("vfs-rename ok request ");
    print_int((int)request_id);
    print_string("\n");
}

static int wait_vfs_mount_reply(int expected_sender, uint32_t type, uint32_t request_id,
                                os_vfs_mount_reply_t* reply) {
    os_ipc_message_t message;
    int rc;
    int attempts;
    rc = os_ipc_deferred_take_matching(&ipc_deferred, type, request_id, &message);
    if (rc == 0 && message.sender_pid != expected_sender) rc = OS_IPC_EMPTY;
    if (rc == 0) return os_vfs_parse_mount_reply(&message, type, reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == type && message.request_id == request_id && message.sender_pid == expected_sender) {
                return os_vfs_parse_mount_reply(&message, type, reply, request_id);
            }
            saved = os_ipc_deferred_push(&ipc_deferred, &message);
            rc = saved == 0 ? OS_IPC_EMPTY : saved;
        }
    }
    return rc;
}

static void cmd_vfs_mount_add(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_vfs_mount_reply_t reply;
    uint32_t source;
    uint32_t request_id;
    int pid;
    int rc;
    if (arg_count != 2) {
        print_error("Usage: vfs-mount-add <prefixe/> <initrd|overlay>");
        return;
    }
    if (strcmp(args[1], "initrd") == 0) source = OS_VFS_MOUNT_SOURCE_INITRD;
    else if (strcmp(args[1], "overlay") == 0) source = OS_VFS_MOUNT_SOURCE_OVERLAY;
    else {
        print_error("vfs-mount-add: source invalide");
        ctx->last_rc = OS_VFS_STATUS_INVALID;
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-mount-add: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_mount_add_request(&request, args[0], source, request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-mount-add: prefixe invalide ou service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = wait_vfs_mount_reply(pid, OS_IPC_VFS_MOUNT_ADD_REPLY, request_id, &reply);
    if (rc != 0) {
        print_error("vfs-mount-add: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_MOUNT_FULL) print_error("vfs-mount-add: table de montages pleine");
        else if (reply.status == OS_VFS_STATUS_MOUNT_EXISTS) print_error("vfs-mount-add: montage deja present");
        else print_error("vfs-mount-add: montage refuse");
        return;
    }
    print_string("vfs-mount-add ok request ");
    print_int((int)request_id);
    print_string("\n");
}

static void cmd_vfs_mount_remove(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_vfs_mount_reply_t reply;
    uint32_t request_id;
    int pid;
    int rc;
    if (arg_count != 1) {
        print_error("Usage: vfs-mount-remove <prefixe/>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-mount-remove: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_mount_remove_request(&request, args[0], request_id);
    if (rc == 0) rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-mount-remove: prefixe invalide ou service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = wait_vfs_mount_reply(pid, OS_IPC_VFS_MOUNT_REMOVE_REPLY, request_id, &reply);
    if (rc != 0) {
        print_error("vfs-mount-remove: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) print_error("vfs-mount-remove: montage absent");
        else print_error("vfs-mount-remove: montage protege ou refuse");
        return;
    }
    print_string("vfs-mount-remove ok request ");
    print_int((int)request_id);
    print_string("\n");
}

static void cmd_vfs_read(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_read_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    uint32_t i;
    if (arg_count != 1) {
        print_error("Usage: vfs-read <chemin>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-read: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_read_request(&request, args[0], request_id);
    if (rc != 0) {
        print_error("vfs-read: chemin invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-read: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_READ_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_read_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_READ_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_read_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-read: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-read: chemin hors montage");
        } else {
            print_error("vfs-read: lecture refusee ou fichier absent");
        }
        return;
    }
    print_string("vfs-read ok ");
    print_int((int)reply.size);
    print_string(" request ");
    print_int((int)request_id);
    print_string(" data ");
    for (i = 0U; i < reply.size; i++) putc((char)reply.data[i]);
    if (reply.size == 0U || reply.data[reply.size - 1U] != '\n') print_string("\n");
}

static void cmd_vfs_mkdir(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1) { print_error("Usage: vfs-mkdir <chemin>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-mkdir: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_mkdir_request(&request, args[0], request_id);
    if (rc != 0) { print_error("vfs-mkdir: chemin invalide ou trop long"); ctx->last_rc = rc; return; }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-mkdir: service indisponible"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_MKDIR_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_mkdir_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved; yield(); rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_MKDIR_REPLY && message.request_id == request_id && message.sender_pid == pid)
                rc = os_vfs_parse_mkdir_reply(&message, &status, request_id);
            else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; }
        }
    }
    if (rc != 0) { print_error("vfs-mkdir: reponse VFS absente ou invalide"); ctx->last_rc = rc; return; }
    ctx->last_rc = status;
    if (status == OS_VFS_STATUS_OK) { print_string("vfs-mkdir ok request "); print_int((int)request_id); print_string("\n"); }
    else if (status == OS_VFS_STATUS_NOT_MOUNTED) print_error("vfs-mkdir: chemin hors montage overlay");
    else print_error("vfs-mkdir: creation refusee");
}

static void cmd_vfs_rmdir(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message;
    int pid, rc, status, attempts; uint32_t request_id;
    if (arg_count != 1) { print_error("Usage: vfs-rmdir <chemin>"); return; }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-rmdir: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id(); rc = os_vfs_make_rmdir_request(&request, args[0], request_id);
    if (rc != 0) { print_error("vfs-rmdir: chemin invalide ou trop long"); ctx->last_rc = rc; return; }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-rmdir: service indisponible"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_RMDIR_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_rmdir_reply(&message, &status, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved; yield(); rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_RMDIR_REPLY && message.request_id == request_id && message.sender_pid == pid)
                rc = os_vfs_parse_rmdir_reply(&message, &status, request_id);
            else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; }
        }
    }
    if (rc != 0) { print_error("vfs-rmdir: reponse VFS absente ou invalide"); ctx->last_rc = rc; return; }
    ctx->last_rc = status;
    if (status == OS_VFS_STATUS_OK) { print_string("vfs-rmdir ok request "); print_int((int)request_id); print_string("\n"); }
    else if (status == OS_VFS_STATUS_NOT_MOUNTED) print_error("vfs-rmdir: chemin hors montage overlay");
    else if (status == OS_VFS_STATUS_NOT_EMPTY) print_error("vfs-rmdir: repertoire non vide");
    else print_error("vfs-rmdir: suppression refusee ou repertoire absent");
}

static void cmd_vfs_list(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_list_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    uint32_t i;
    if (arg_count != 1) {
        print_error("Usage: vfs-list <repertoire/>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-list: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_list_request(&request, args[0], request_id);
    if (rc != 0) {
        print_error("vfs-list: repertoire invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-list: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_LIST_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_list_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_LIST_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_list_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-list: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK && reply.status != OS_VFS_STATUS_TRUNCATED) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-list: repertoire hors montage");
        } else {
            print_error("vfs-list: listage refuse");
        }
        return;
    }
    print_string("vfs-list ");
    print_string(reply.status == OS_VFS_STATUS_TRUNCATED ? "partiel" : "ok");
    print_string(" count ");
    print_int((int)reply.count);
    print_string(" request ");
    print_int((int)request_id);
    print_string("\n");
    for (i = 0U; i < OS_VFS_LIST_DATA_MAX && reply.data[i] != 0U; i++) {
        putc((char)reply.data[i]);
    }
    if (i == 0U || reply.data[i - 1U] != '\n') print_string("\n");
}

static void cmd_vfs_list_page(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_list_page_reply_t reply;
    int pid;
    int rc;
    int attempts;
    int start;
    uint32_t request_id;
    uint32_t i;
    if (arg_count != 2 || (start = parse_int(args[1])) < 0) {
        print_error("Usage: vfs-list-page <repertoire/> <depart>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-list-page: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_list_page_request(&request, args[0], (uint32_t)start, request_id);
    if (rc != 0) { print_error("vfs-list-page: repertoire ou index invalide"); ctx->last_rc = rc; return; }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-list-page: service indisponible"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_LIST_PAGE_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_list_page_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_LIST_PAGE_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_list_page_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) { print_error("vfs-list-page: reponse VFS absente ou invalide"); ctx->last_rc = rc; return; }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK && reply.status != OS_VFS_STATUS_TRUNCATED) {
        print_error(reply.status == OS_VFS_STATUS_NOT_MOUNTED ?
                    "vfs-list-page: repertoire hors montage" : "vfs-list-page: listage refuse");
        return;
    }
    print_string("vfs-list-page ");
    print_string(reply.status == OS_VFS_STATUS_TRUNCATED ? "partiel" : "ok");
    print_string(" count "); print_int((int)reply.count);
    print_string(" next ");
    if (reply.next_start == OS_VFS_LIST_PAGE_END) print_string("end");
    else print_int((int)reply.next_start);
    print_string(" request "); print_int((int)request_id); print_string("\n");
    for (i = 0U; i < OS_VFS_LIST_PAGE_DATA_MAX && reply.data[i] != 0U; i++) putc((char)reply.data[i]);
    if (i == 0U || reply.data[i - 1U] != '\n') print_string("\n");
}

static void cmd_vfs_list_observe(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request; os_ipc_message_t message; os_vfs_list_observe_reply_t reply;
    int pid, rc, attempts, start, generation; uint32_t request_id, i;
    if (arg_count != 3 || (start = parse_int(args[1])) < 0 || (generation = parse_int(args[2])) < 0) {
        print_error("Usage: vfs-list-observe <repertoire/> <depart> <generation>"); return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) { print_error("vfs-list-observe: service vfs indisponible"); ctx->last_rc = pid; return; }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_list_observe_request(&request, args[0], (uint32_t)start, (uint32_t)generation, request_id);
    if (rc != 0) { print_error("vfs-list-observe: argument invalide"); ctx->last_rc = rc; return; }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) { print_error("vfs-list-observe: service indisponible"); ctx->last_rc = rc; return; }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_LIST_OBSERVE_REPLY, request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_list_observe_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved; yield(); rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_LIST_OBSERVE_REPLY && message.request_id == request_id && message.sender_pid == pid)
                rc = os_vfs_parse_list_observe_reply(&message, &reply, request_id);
            else { saved = os_ipc_deferred_push(&ipc_deferred, &message); rc = saved == 0 ? OS_IPC_EMPTY : saved; }
        }
    }
    if (rc != 0) { print_error("vfs-list-observe: reponse VFS absente ou invalide"); ctx->last_rc = rc; return; }
    ctx->last_rc = reply.status;
    if (reply.status == OS_VFS_STATUS_STALE) {
        print_string("vfs-list-observe obsolete generation "); print_int((int)reply.generation); print_string("\n"); return;
    }
    if (reply.status != OS_VFS_STATUS_OK && reply.status != OS_VFS_STATUS_TRUNCATED) {
        print_error("vfs-list-observe: listage refuse"); return;
    }
    print_string("vfs-list-observe "); print_string(reply.status == OS_VFS_STATUS_TRUNCATED ? "partiel" : "ok");
    print_string(" count "); print_int((int)reply.count); print_string(" next ");
    if (reply.next_start == OS_VFS_LIST_PAGE_END) print_string("end"); else print_int((int)reply.next_start);
    print_string(" generation "); print_int((int)reply.generation); print_string(" request "); print_int((int)request_id); print_string("\n");
    for (i = 0U; i < OS_VFS_LIST_OBSERVE_DATA_MAX && reply.data[i] != 0U; i++) putc((char)reply.data[i]);
    if (i == 0U || reply.data[i - 1U] != '\n') print_string("\n");
}

static void cmd_vfs_stat(shell_context_t* ctx, char args[][128], int arg_count) {
    os_ipc_payload_t request;
    os_ipc_message_t message;
    os_vfs_stat_reply_t reply;
    int pid;
    int rc;
    int attempts;
    uint32_t request_id;
    if (arg_count != 1) {
        print_error("Usage: vfs-stat <chemin>");
        return;
    }
    pid = sys_service_lookup("vfs");
    if (pid <= 0) {
        print_error("vfs-stat: service vfs indisponible");
        ctx->last_rc = pid;
        return;
    }
    request_id = next_vfs_request_id();
    rc = os_vfs_make_stat_request(&request, args[0], request_id);
    if (rc != 0) {
        print_error("vfs-stat: chemin invalide ou trop long");
        ctx->last_rc = rc;
        return;
    }
    rc = sys_ipc_send(pid, &request);
    if (rc != 0) {
        print_error("vfs-stat: service indisponible");
        ctx->last_rc = rc;
        return;
    }
    rc = os_ipc_deferred_take_matching(&ipc_deferred, OS_IPC_VFS_STAT_REPLY,
                                       request_id, &message);
    if (rc == 0 && message.sender_pid != pid) rc = OS_IPC_EMPTY;
    if (rc == 0) rc = os_vfs_parse_stat_reply(&message, &reply, request_id);
    for (attempts = 0; attempts < 3 && rc == OS_IPC_EMPTY; attempts++) {
        int saved;
        yield();
        rc = sys_ipc_receive(&message);
        if (rc == 0) {
            if (message.type == OS_IPC_VFS_STAT_REPLY && message.request_id == request_id && message.sender_pid == pid) {
                rc = os_vfs_parse_stat_reply(&message, &reply, request_id);
            } else {
                saved = os_ipc_deferred_push(&ipc_deferred, &message);
                rc = saved == 0 ? OS_IPC_EMPTY : saved;
            }
        }
    }
    if (rc != 0) {
        print_error("vfs-stat: reponse VFS absente ou invalide");
        ctx->last_rc = rc;
        return;
    }
    ctx->last_rc = reply.status;
    if (reply.status != OS_VFS_STATUS_OK) {
        if (reply.status == OS_VFS_STATUS_NOT_MOUNTED) {
            print_error("vfs-stat: chemin hors montage");
        } else {
            print_error("vfs-stat: metadonnees refusees ou fichier absent");
        }
        return;
    }
    print_string("vfs-stat ok size ");
    print_int((int)reply.size);
    print_string(" flags ");
    print_string(reply.flags == OS_DIRENT_DIR ? "dir" : "file");
    print_string(" request ");
    print_int((int)request_id);
    print_string("\n");
}

static void cmd_vfs_stats(shell_context_t* ctx, char args[][128], int arg_count) {
    char stats_args[1][128] = { "vfs-stats" };
    (void)args;
    if (arg_count != 0) {
        print_error("Usage: vfs-stats");
        return;
    }
    cmd_vfs_read(ctx, stats_args, 1);
}

static void cmd_yield(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)args;
    (void)arg_count;
    yield();
    print_string("yield ok\n");
    ctx->last_rc = 0;
}

static void cmd_jobs(shell_context_t* ctx, char args[][128], int arg_count) {
    os_proc_t procs[16];
    int n;
    int shown = 0;
    (void)ctx; (void)args; (void)arg_count;
    n = sys_ps(procs, 16);
    print_colored("\n=== Jobs ===\n", COLOR_CYAN);
    for (int i = 0; i < n; i++) {
        if (procs[i].type != OS_TASK_USER) continue;
        print_string("[");
        print_int(procs[i].pid);
        print_string("]  ");
        print_string(proc_state_str(procs[i].state));
        print_string("  ");
        print_string(procs[i].name);
        print_string("\n");
        shown++;
    }
    if (shown == 0) print_string("Aucun job utilisateur.\n");
    print_string("jobs ok ");
    print_int(shown);
    print_string("\n");
}

static void cmd_top(shell_context_t* ctx, char args[][128], int arg_count) {
    os_proc_t procs[16];
    int n;
    (void)args; (void)arg_count;
    n = sys_ps(procs, 16);
    print_colored("\n=== top (noyau) ===\n", COLOR_CYAN);
    print_string("ticks: ");
    print_int((int)sys_ticks());
    print_string("  pid: ");
    print_int(sys_getpid());
    print_string("  tasks: ");
    print_int(n < 0 ? 0 : n);
    print_string("\n");
    print_colored("  PID  STAT  TYPE  COMMAND\n", COLOR_YELLOW);
    for (int i = 0; i < n; i++) {
        print_string("  ");
        print_int(procs[i].pid);
        print_string("    ");
        print_string(proc_state_str(procs[i].state));
        print_string("     ");
        print_string(procs[i].type == OS_TASK_USER ? "user  " : "kern  ");
        print_string(procs[i].name);
        print_string("\n");
    }
    print_string("top ok ");
    print_int(n < 0 ? 0 : n);
    print_string("\n");
}

static void cmd_getpid(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)ctx; (void)args; (void)arg_count;
    print_string("getpid ok ");
    print_int(sys_getpid());
    print_string("\n");
}

static void cmd_uptime(shell_context_t* ctx, char args[][128], int arg_count) {
    unsigned int ticks = sys_ticks();
    int sec = (int)(ticks / 100);
    int h, m, s;
    (void)ctx; (void)args; (void)arg_count;
    if (sec < 0) sec = 0;
    h = sec / 3600;
    m = (sec % 3600) / 60;
    s = sec % 60;
    print_string("up ");
    print_int(h);
    print_string(":");
    if (m < 10) putc('0');
    print_int(m);
    print_string(":");
    if (s < 10) putc('0');
    print_int(s);
    print_string("  (PIT ticks: ");
    print_int((int)ticks);
    print_string(")\n");
}

static void cmd_date(shell_context_t* ctx, char args[][128], int arg_count) {
    unsigned int ticks = sys_ticks();
    int sec = (int)((ticks / 100) % 86400);
    int h = 5 + (sec / 3600);
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    (void)ctx; (void)args; (void)arg_count;
    if (h >= 24) h %= 24;
    print_string("Thu Aug 13 ");
    if (h < 10) putc('0');
    print_int(h);
    print_string(":");
    if (m < 10) putc('0');
    print_int(m);
    print_string(":");
    if (s < 10) putc('0');
    print_int(s);
    print_string(" UTC 2026\n");
    print_string("date ok\n");
}

static void cmd_whoami(shell_context_t* ctx, char args[][128], int arg_count) {
    const char* user = get_env_var(ctx, "USER");
    (void)args; (void)arg_count;
    print_string("whoami ok ");
    print_string(user ? user : "root");
    print_string("\n");
}

static void cmd_alias(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        if (ctx->alias_count == 0) {
            print_string("Aucun alias.\n");
            return;
        }
        for (int i = 0; i < ctx->alias_count; i++) {
            print_string("alias ");
            print_string(ctx->aliases[i].alias);
            print_string("='");
            print_string(ctx->aliases[i].command);
            print_string("'\n");
        }
        return;
    }
    {
        char name[64];
        char value[256];
        char* eq = find_char(args[0], '=');
        name[0] = 0;
        value[0] = 0;
        if (eq) {
            int nlen = (int)(eq - args[0]);
            int i;
            if (nlen <= 0 || nlen >= 63) {
                print_error("alias: nom invalide");
                return;
            }
            for (i = 0; i < nlen; i++) name[i] = args[0][i];
            name[nlen] = 0;
            strcpy(value, eq + 1);
            for (int a = 1; a < arg_count; a++) {
                strcat(value, " ");
                strcat(value, args[a]);
            }
        } else if (arg_count >= 2) {
            strcpy(name, args[0]);
            strcpy(value, args[1]);
            for (int a = 2; a < arg_count; a++) {
                strcat(value, " ");
                strcat(value, args[a]);
            }
        } else {
            print_error("alias: usage alias nom=commande");
            return;
        }
        for (int i = 0; i < ctx->alias_count; i++) {
            if (strcmp(ctx->aliases[i].alias, name) == 0) {
                strcpy(ctx->aliases[i].command, value);
                print_string("alias ok ");
                print_string(name);
                print_string("\n");
                return;
            }
        }
        if (ctx->alias_count >= MAX_ENV_VARS) {
            print_error("alias: table pleine");
            return;
        }
        strcpy(ctx->aliases[ctx->alias_count].alias, name);
        strcpy(ctx->aliases[ctx->alias_count].command, value);
        ctx->alias_count++;
        print_string("alias ok ");
        print_string(name);
        print_string("\n");
    }
}

static void cmd_unalias(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_error("unalias: nom manquant");
        return;
    }
    for (int i = 0; i < ctx->alias_count; i++) {
        if (strcmp(ctx->aliases[i].alias, args[0]) == 0) {
            for (int j = i; j < ctx->alias_count - 1; j++) {
                ctx->aliases[j] = ctx->aliases[j + 1];
            }
            ctx->alias_count--;
            print_string("unalias ok ");
            print_string(args[0]);
            print_string("\n");
            return;
        }
    }
    print_error("unalias: alias introuvable");
}

static void cmd_export(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        cmd_env(ctx, args, arg_count);
        return;
    }
    {
        char* eq = find_char(args[0], '=');
        if (eq) {
            char name[64];
            int nlen = (int)(eq - args[0]);
            int i;
            if (nlen <= 0 || nlen >= 63) {
                print_error("export: nom invalide");
                return;
            }
            for (i = 0; i < nlen; i++) name[i] = args[0][i];
            name[nlen] = 0;
            set_env_var(ctx, name, eq + 1);
            print_string("export ok ");
            print_string(name);
            print_string("\n");
        } else if (arg_count >= 2) {
            set_env_var(ctx, args[0], args[1]);
            print_string("export ok ");
            print_string(args[0]);
            print_string("\n");
        } else {
            print_error("export: usage export VAR=valeur");
        }
    }
}

static int load_file_lines(shell_context_t* ctx, const char* filearg,
                           char lines[][128], int max_lines) {
    char path[RAMFS_PATH_MAX];
    char kbuf[1024];
    const char* data;
    int size = 0;
    int pos = 0;
    int n = 0;
    int kn;
    resolve_arg(ctx, filearg, path);
    kn = sys_readfile(path, kbuf, (int)sizeof(kbuf));
    if (kn >= 0) {
        data = kbuf;
        size = kn;
    } else {
        data = ramfs_read(path, &size);
        if (!data) return -1;
    }
    while (pos < size && n < max_lines) {
        int len = 0;
        while (pos < size && data[pos] != '\n' && len < 127) {
            lines[n][len++] = data[pos++];
        }
        lines[n][len] = 0;
        if (pos < size && data[pos] == '\n') pos++;
        n++;
    }
    return n;
}

static void cmd_grep(shell_context_t* ctx, char args[][128], int arg_count) {
    char lines[32][128];
    int n;
    int hits = 0;
    if (arg_count < 2) {
        print_error("grep: usage grep <motif> <fichier>");
        return;
    }
    n = load_file_lines(ctx, args[1], lines, 32);
    if (n < 0) {
        print_error("grep: fichier introuvable");
        return;
    }
    for (int i = 0; i < n; i++) {
        if (strstr(lines[i], args[0])) {
            print_string(lines[i]);
            print_string("\n");
            hits++;
        }
    }
    if (hits == 0) {
        print_string("(aucune correspondance)\n");
        return;
    }
    print_string("grep hits ");
    print_int(hits);
    print_string("\n");
}

static void cmd_wc(shell_context_t* ctx, char args[][128], int arg_count) {
    char path[RAMFS_PATH_MAX];
    char kbuf[1024];
    const char* data;
    int size = 0;
    int lines = 0, words = 0, chars = 0;
    int in_word = 0;
    int kn;
    if (arg_count == 0) {
        print_error("wc: fichier manquant");
        return;
    }
    resolve_arg(ctx, args[0], path);
    kn = sys_readfile(path, kbuf, (int)sizeof(kbuf));
    if (kn >= 0) {
        data = kbuf;
        size = kn;
    } else {
        data = ramfs_read(path, &size);
        if (!data) {
            print_error("wc: fichier introuvable");
            return;
        }
    }
    chars = size;
    for (int i = 0; i < size; i++) {
        char c = data[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') in_word = 0;
        else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    print_string("wc ok ");
    print_int(lines);
    print_string(" ");
    print_int(words);
    print_string(" ");
    print_int(chars);
    print_string(" ");
    print_string(args[0]);
    print_string("\n");
}

static void cmd_sort(shell_context_t* ctx, char args[][128], int arg_count) {
    char lines[32][128];
    int n;
    if (arg_count == 0) {
        print_error("sort: fichier manquant");
        return;
    }
    n = load_file_lines(ctx, args[0], lines, 32);
    if (n < 0) {
        print_error("sort: fichier introuvable");
        return;
    }
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (strcmp(lines[j], lines[j + 1]) > 0) {
                char tmp[128];
                strcpy(tmp, lines[j]);
                strcpy(lines[j], lines[j + 1]);
                strcpy(lines[j + 1], tmp);
            }
        }
    }
    for (int i = 0; i < n; i++) {
        print_string(lines[i]);
        print_string("\n");
    }
    print_string("sort ok ");
    print_int(n);
    print_string(" ");
    print_string(args[0]);
    print_string("\n");
}

static int parse_line_count(char args[][128], int arg_count, int* file_idx) {
    int n = 10;
    *file_idx = 0;
    if (arg_count >= 2 && strcmp(args[0], "-n") == 0) {
        n = parse_int(args[1]);
        *file_idx = 2;
    } else if (arg_count >= 1 && args[0][0] == '-' && args[0][1] >= '0' && args[0][1] <= '9') {
        n = parse_int(args[0] + 1);
        *file_idx = 1;
    }
    if (n < 0) n = 0;
    if (n > 32) n = 32;
    return n;
}

static void cmd_head(shell_context_t* ctx, char args[][128], int arg_count) {
    char lines[32][128];
    int file_idx = 0;
    int want;
    int n;
    if (arg_count == 0) {
        print_error("head: fichier manquant");
        return;
    }
    want = parse_line_count(args, arg_count, &file_idx);
    if (file_idx >= arg_count) {
        print_error("head: fichier manquant");
        return;
    }
    n = load_file_lines(ctx, args[file_idx], lines, 32);
    if (n < 0) {
        print_error("head: fichier introuvable");
        return;
    }
    if (want > n) want = n;
    for (int i = 0; i < want; i++) {
        print_string(lines[i]);
        print_string("\n");
    }
    print_string("head ok ");
    print_int(want);
    print_string(" ");
    print_string(args[file_idx]);
    print_string("\n");
}

static void cmd_tail(shell_context_t* ctx, char args[][128], int arg_count) {
    char lines[32][128];
    int file_idx = 0;
    int want;
    int n;
    int start;
    if (arg_count == 0) {
        print_error("tail: fichier manquant");
        return;
    }
    want = parse_line_count(args, arg_count, &file_idx);
    if (file_idx >= arg_count) {
        print_error("tail: fichier manquant");
        return;
    }
    n = load_file_lines(ctx, args[file_idx], lines, 32);
    if (n < 0) {
        print_error("tail: fichier introuvable");
        return;
    }
    if (want > n) want = n;
    start = n - want;
    for (int i = start; i < n; i++) {
        print_string(lines[i]);
        print_string("\n");
    }
    print_string("tail ok ");
    print_int(want);
    print_string(" ");
    print_string(args[file_idx]);
    print_string("\n");
}

static const char* ai_provider_name(const shell_context_t* ctx) {
    return ctx->ai_provider == AI_PROVIDER_OPENAI ? "openai" : "local";
}

static const char* ai_model_name(const shell_context_t* ctx) {
    return ctx->ai_model;
}

static void cmd_ai_stats(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)args; (void)arg_count;
    print_colored("\n=== Statistiques IA ===\n", COLOR_CYAN);
    print_string("Requêtes ai : ");
    print_int(ctx->ai_query_count);
    print_string("\nMode IA     : ");
    print_string(ctx->ai_mode ? "active" : "desactive");
    print_string("\nFournisseur : ");
    print_string(ai_provider_name(ctx));
    print_string("\nModele      : ");
    print_string(ai_model_name(ctx));
    print_string("\nMoteur      : GPT-2 local bare-metal avec echantillonnage top-k\n");
    print_string("aistats ok ");
    print_int(ctx->ai_query_count);
    print_string("\n");
}

static void cmd_rc(shell_context_t* ctx) {
    print_string("rc ok ");
    print_int(ctx->last_rc);
    print_string("\n");
}

static void cmd_ai_provider(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_string("Fournisseur IA : ");
        print_string(ai_provider_name(ctx));
        print_string("\nUsage: ai-provider [local|openai]\n");
        return;
    }
    if (strcmp(args[0], "local") == 0) {
        ctx->ai_provider = AI_PROVIDER_LOCAL;
        print_success("Fournisseur local selectionne");
        return;
    }
    if (strcmp(args[0], "openai") == 0) {
        ctx->ai_provider = AI_PROVIDER_OPENAI;
        print_warning("OpenAI selectionne : pilote reseau, DNS et TLS requis avant appel reel");
        return;
    }
    print_error("Usage: ai-provider [local|openai]");
}

static void cmd_ai_model(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0 || strcmp(args[0], "status") == 0) {
        print_string("Modele local courant : ");
        print_string(ai_model_name(ctx));
        print_string("\nUsage: ai-model [list|use <modele.bin|modele.gguf>]\n");
        return;
    }
    if (strcmp(args[0], "list") == 0) {
        print_string("Modeles locaux declares :\n");
        print_string("  gpt2_124M.bin  [operationnel : checkpoint llm.c v3, CPU bare-metal]\n");
        print_string("  qwen2.5-1.5b-instruct-q4_0.gguf  [profil futur : chargeur GGUF requis]\n");
        return;
    }
    if (strcmp(args[0], "use") == 0 && arg_count == 2) {
        if (strstr(args[1], ".bin") == 0 && strstr(args[1], ".gguf") == 0) {
            print_error("Le profil local doit pointer vers un fichier .bin ou .gguf");
            return;
        }
        strcpy(ctx->ai_model, args[1]);
        if (strstr(args[1], "gpt2") != 0) {
            print_success("Profil GPT-2 selectionne; validation par le chargeur au boot");
        } else {
            print_warning("Profil memorise; seul GPT-2 .bin est actuellement executable");
        }
        return;
    }
    print_error("Usage: ai-model [list|use <modele.bin|modele.gguf>]");
}

static void cmd_ai_runtime(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)args; (void)arg_count;
    print_colored("\n=== Runtime IA bare-metal ===\n", COLOR_CYAN);
    print_string("Architecture active : PC i386 Multiboot, CPU, 1 Gio RAM requis pour GPT-2\n");
    print_string("Fournisseur actif  : ");
    print_string(ai_provider_name(ctx));
    print_string("\nModele declare     : ");
    print_string(ai_model_name(ctx));
    print_string("\nLocal              : GPT-2 124M .bin + tokenizer .bin, generation top-k\n");
    print_string("Limite locale      : 64 jetons de contexte, 4 jetons generes, cache KV actif\n");
    print_string("En ligne           : pilote Ethernet, TCP/IP, DNS et TLS a integrer\n");
    print_string("Secrets OpenAI     : jamais integres a l'image de boot\n\n");
}

static void cmd_net_status(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)ctx; (void)args; (void)arg_count;
    print_colored("\n=== Reseau bare-metal ===\n", COLOR_CYAN);
    print_string("Carte Ethernet : absente (aucun pilote NIC initialise)\n");
    print_string("ARP / IPv4 / DHCP : absents\n");
    print_string("DNS / TCP / TLS   : absents\n");
    print_string("OpenAI en ligne   : bloque, aucune requete n'est emise\n");
    print_string("net-status ok stub AOS-025\n");
}

static void cmd_reboot(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)ctx; (void)args; (void)arg_count;
    print_warning("reboot: simule (QEMU reste actif, tapez exit pour quitter le shell)");
}

static void cmd_shutdown(shell_context_t* ctx, char args[][128], int arg_count) {
    (void)ctx; (void)args; (void)arg_count;
    print_warning("shutdown: simule (QEMU reste actif, tapez exit pour quitter le shell)");
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
    if (ctx->ai_provider == AI_PROVIDER_OPENAI) {
        print_colored("[IA] OpenAI configure mais indisponible : reseau/TLS bare-metal non integres\n", COLOR_YELLOW);
        return;
    }
    print_colored("[IA] profil local : ", COLOR_CYAN);
    print_string(ai_model_name(ctx));
    print_string("\n");
    if (strstr(ai_model_name(ctx), "gpt2") != 0) {
        char generated[384];
        int generated_len = sys_gpt2_generate(query, generated, sizeof(generated));
        if (generated_len >= 0) {
            print_colored("[GPT-2 local] ", COLOR_GREEN);
            if (generated_len == 0) print_string("(fin de sequence)");
            else print_string(generated);
            print_string("\n");
            return;
        }
        print_colored("[GPT-2 local] indisponible (code ", COLOR_YELLOW);
        print_int(generated_len);
        print_colored("); repli de compatibilite.\n", COLOR_YELLOW);
    }
    // Lancer le binaire local de compatibilite en tache bloquante pour garantir l'affichage
    char* argv[3];
    argv[0] = "ai_assistant";
    argv[1] = (char*)query;
    argv[2] = 0;
    // Essayer d'abord dans bin/ en mode bloquant pour garantir l'affichage
    int rc = exec("bin/ai_assistant", argv);
    if (rc != 0) {
        rc = exec("ai_assistant", argv);
    }
    if (rc != 0) {
        print_colored("\n[IA] indisponible\n", COLOR_YELLOW);
        return;
    }
    print_string("ai ok\n");
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
    // Echo immediate for visibility
    print_colored("[IA] ", COLOR_MAGENTA);
    print_string("question: ");
    print_string(full_query);
    print_string("\n");
    ctx->ai_query_count++;
    call_ai_assistant(ctx, full_query);
}

void cmd_ai_mode(shell_context_t* ctx, char args[][128], int arg_count) {
    if (arg_count == 0) {
        print_string("aimode ok ");
        print_string(ctx->ai_mode ? "on" : "off");
        print_string("\n");
        return;
    }
    
    if (strcmp(args[0], "on") == 0) {
        ctx->ai_mode = 1;
        print_string("aimode ok on\n");
    } else if (strcmp(args[0], "off") == 0) {
        ctx->ai_mode = 0;
        print_string("aimode ok off\n");
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
    print_string("aihelp ok\n");
}

// Test IA: lance l'IA avec une requete de sante et verifie le code retour
static void cmd_ai_test(shell_context_t* ctx) {
    char* argv[3];
    int rc;
    argv[0] = "ai_assistant";
    argv[1] = "healthcheck";
    argv[2] = 0;
    rc = exec("bin/ai_assistant", argv);
    if (rc != 0) rc = exec("ai_assistant", argv);
    if (rc != 0) {
        print_string("aitest fail\n");
        ctx->last_rc = 1;
        return;
    }
    print_string("aitest ok\n");
    ctx->last_rc = 0;
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
    } else if (strcmp(command, "task-metrics") == 0) {
        cmd_task_metrics(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "task-priority") == 0) {
        cmd_task_priority(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "task-name") == 0) {
        cmd_task_name(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "task-capacity") == 0) {
        cmd_task_capacity(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-result") == 0) {
        cmd_child_result(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-result-any") == 0) {
        cmd_child_result_any(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-results") == 0) {
        cmd_child_results(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-results-forget") == 0) {
        cmd_child_results_forget(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-results-clear") == 0) {
        cmd_child_results_clear(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "child-results-observe") == 0) {
        cmd_child_results_observe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "wait") == 0) {
        cmd_task_wait(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "wait-result") == 0) {
        cmd_wait_result(ctx, args, arg_count);
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
    } else if (strcmp(command, "write") == 0) {
        cmd_write(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "append") == 0) {
        cmd_append(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "touch") == 0) {
        cmd_touch(ctx, args, arg_count);
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
    } else if (strcmp(command, "stat") == 0) {
        cmd_stat(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "test") == 0 || strcmp(command, "[") == 0) {
        int n = arg_count;
        if (command[0] == '[' && n > 0 && strcmp(args[n - 1], "]") == 0)
            n--;
        cmd_test(ctx, args, n);
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
    } else if (strcmp(command, "ai-mode") == 0 || strcmp(command, "aimode") == 0) {
        cmd_ai_mode(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-help") == 0 || strcmp(command, "aihelp") == 0) {
        cmd_ai_help(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-test") == 0 || strcmp(command, "aitest") == 0) {
        cmd_ai_test(ctx);
        return 1;
    } else if (strcmp(command, "mkdir") == 0) {
        cmd_mkdir(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "rmdir") == 0) {
        cmd_rmdir(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "rm") == 0) {
        cmd_rm(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "cp") == 0) {
        cmd_cp(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "mv") == 0) {
        cmd_mv(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "kill") == 0) {
        cmd_kill(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "spawn") == 0) {
        cmd_spawn(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ipc-send") == 0) {
        cmd_ipc_send(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ipc-recv") == 0) {
        cmd_ipc_recv(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "service-publish") == 0) {
        cmd_service_publish(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "service-grant") == 0) {
        cmd_service_grant(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "service-find") == 0) {
        cmd_service_find(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "service-status") == 0) {
        cmd_service_status(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "service-watch") == 0) {
        cmd_service_watch(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-probe") == 0) {
        cmd_vfs_backend_probe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-write-probe") == 0) {
        cmd_vfs_backend_write_probe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-remove-probe") == 0) {
        cmd_vfs_backend_remove_probe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-rename-probe") == 0) {
        cmd_vfs_backend_rename_probe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-grant") == 0) {
        cmd_vfs_grant(ctx, args, arg_count);
    } else if (strcmp(command, "vfs-backend-grant") == 0) {
        cmd_vfs_backend_grant(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-grant-read") == 0) {
        cmd_vfs_backend_grant_read(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-grant-mutate") == 0) {
        cmd_vfs_backend_grant_mutate(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-revoke") == 0) {
        cmd_vfs_backend_revoke(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-status") == 0) {
        cmd_vfs_backend_status(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-observe") == 0) {
        cmd_vfs_backend_observe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-backend-list") == 0) {
        cmd_vfs_backend_list(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-read") == 0) {
        cmd_vfs_read(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-stat") == 0) {
        cmd_vfs_stat(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-mkdir") == 0) {
        cmd_vfs_mkdir(ctx, args, arg_count);
    } else if (strcmp(command, "vfs-rmdir") == 0) {
        cmd_vfs_rmdir(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-list") == 0) {
        cmd_vfs_list(ctx, args, arg_count);
    } else if (strcmp(command, "vfs-list-page") == 0) {
        cmd_vfs_list_page(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-list-observe") == 0) {
        cmd_vfs_list_observe(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-stats") == 0) {
        cmd_vfs_stats(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-mount-add") == 0) {
        cmd_vfs_mount_add(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-mount-remove") == 0) {
        cmd_vfs_mount_remove(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-write") == 0) {
        cmd_vfs_write(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-remove") == 0) {
        cmd_vfs_remove(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "vfs-rename") == 0) {
        cmd_vfs_rename(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "yield") == 0) {
        cmd_yield(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "jobs") == 0) {
        cmd_jobs(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "top") == 0) {
        cmd_top(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "getpid") == 0) {
        cmd_getpid(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "uptime") == 0) {
        cmd_uptime(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "date") == 0) {
        cmd_date(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "whoami") == 0) {
        cmd_whoami(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "alias") == 0) {
        cmd_alias(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "unalias") == 0) {
        cmd_unalias(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "export") == 0) {
        cmd_export(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "grep") == 0) {
        cmd_grep(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "wc") == 0) {
        cmd_wc(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "sort") == 0) {
        cmd_sort(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "head") == 0) {
        cmd_head(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "tail") == 0) {
        cmd_tail(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-stats") == 0 || strcmp(command, "aistats") == 0) {
        cmd_ai_stats(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "rc") == 0) {
        cmd_rc(ctx);
        return 1;
    } else if (strcmp(command, "ai-provider") == 0) {
        cmd_ai_provider(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-model") == 0) {
        cmd_ai_model(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "ai-runtime") == 0) {
        cmd_ai_runtime(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "net-status") == 0) {
        cmd_net_status(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "logout") == 0) {
        cmd_exit(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "reboot") == 0) {
        cmd_reboot(ctx, args, arg_count);
        return 1;
    } else if (strcmp(command, "shutdown") == 0) {
        cmd_shutdown(ctx, args, arg_count);
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

    ctx->cmd_ticks++;

    // Expansion d'alias (une seule fois)
    for (int i = 0; i < ctx->alias_count; i++) {
        if (strcmp(command, ctx->aliases[i].alias) == 0) {
            char rebuilt[MAX_COMMAND_LENGTH];
            int pos = 0;
            const char* ac = ctx->aliases[i].command;
            int j = 0;
            while (ac[j] && pos < MAX_COMMAND_LENGTH - 2) rebuilt[pos++] = ac[j++];
            for (int a = 0; a < arg_count; a++) {
                if (pos < MAX_COMMAND_LENGTH - 2) rebuilt[pos++] = ' ';
                j = 0;
                while (args[a][j] && pos < MAX_COMMAND_LENGTH - 2) rebuilt[pos++] = args[a][j++];
            }
            rebuilt[pos] = '\0';
            parse_command(rebuilt, command, args, &arg_count);
            break;
        }
    }

    // Exécuter la commande builtin
    if (execute_builtin_command(ctx, command, args, arg_count)) {
        if (strcmp(command, "rc") != 0
            && strcmp(command, "test") != 0
            && strcmp(command, "[") != 0
            && strcmp(command, "ai-test") != 0
            && strcmp(command, "aitest") != 0)
            ctx->last_rc = 0;
        return;
    }

    // Si pas de commande builtin, essayer d'exécuter un programme externe
    char* exec_args[MAX_ARGS + 2];
    exec_args[0] = command;
    for (int i = 0; i < arg_count; i++) {
        exec_args[i + 1] = args[i];
    }
    exec_args[arg_count + 1] = NULL;

    // Résolution simple PATH: essayer tel quel (bloquant), puis bin/<cmd>
    int result = exec(command, exec_args);
    if (result != 0) {
        char alt[MAX_PATH_LENGTH];
        strcpy(alt, "bin/");
        strcat(alt, command);
        result = exec(alt, exec_args);
    }

    if (result != 0) {
        ctx->last_rc = 1;
        print_error("Commande non trouvée ou erreur d'exécution");
        print_string("   Tapez 'help' pour voir les commandes disponibles\n");
        
        // Suggestion IA si mode activé
        if (ctx->ai_mode) {
            print_colored("💡 Suggestion IA : ", COLOR_YELLOW);
            print_string("Voulez-vous que je vous aide avec cette commande ?\n");
        }
    } else {
        ctx->last_rc = 0;
    }
}

void shell_main_loop(shell_context_t* ctx) {
    char buf[MAX_COMMAND_LENGTH];

    while (1) {
        display_prompt(ctx);
        buf[0] = '\0';
        // Lecture bloquante et stable de la ligne par le noyau
        gets(buf, (int)sizeof(buf));
        handle_line(ctx, buf);
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