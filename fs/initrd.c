#include "initrd.h"

// Fonctions externes (définies dans kernel.c)
extern void print_string_serial(const char* str);
extern void print_string_vga(const char* str, char color);

// Variables globales
initrd_t* current_initrd = 0;
static initrd_file_t files_array[64]; // Support jusqu'à 64 fichiers

// Fonctions utilitaires pour les chaînes de caractères
int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

// Convertit une chaîne octale en entier
int oct2bin(char *str, int size) {
    int n = 0;
    char *c = str;
    while (size-- > 0 && *c >= '0' && *c <= '7') {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

// Vérifie le checksum d'un en-tête TAR
int tar_checksum_valid(tar_header_t* header) {
    unsigned int sum = 0;
    char* ptr = (char*)header;
    
    // Calcule la somme de tous les octets de l'en-tête
    for (int i = 0; i < 512; i++) {
        if (i >= 148 && i < 156) {
            // Pour le champ checksum, utilise des espaces
            sum += ' ';
        } else {
            sum += (unsigned char)ptr[i];
        }
    }
    
    // Compare avec le checksum stocké
    int stored_checksum = oct2bin(header->checksum, 8);
    return (sum == (unsigned int)stored_checksum);
}

// Initialise le système initrd
void initrd_init(uint32_t location, uint32_t size) {
    if (!location || !size) {
        print_string_serial("Erreur: initrd invalide\n");
        return;
    }
    
    // Alloue la structure initrd (statiquement pour simplifier)
    static initrd_t initrd_struct;
    current_initrd = &initrd_struct;
    
    current_initrd->location = location;
    current_initrd->size = size;
    current_initrd->file_count = 0;
    current_initrd->files = files_array;
    
    // Parse l'archive TAR
    uint32_t current_pos = location;
    uint32_t file_index = 0;
    
    print_string_serial("Parsing initrd TAR archive...\n");
    
    while (current_pos < location + size && file_index < 64) {
        tar_header_t* header = (tar_header_t*)current_pos;
        
        // Vérifie si on a atteint la fin (deux blocs de zéros)
        if (header->name[0] == '\0') {
            break;
        }
        
        // Vérifie la signature TAR
        if (header->magic[0] != 'u' || header->magic[1] != 's' || 
            header->magic[2] != 't' || header->magic[3] != 'a' || 
            header->magic[4] != 'r') {
            print_string_serial("Warning: Invalid TAR magic, skipping\n");
            current_pos += 512;
            continue;
        }
        
        // Vérifie le checksum
        if (!tar_checksum_valid(header)) {
            print_string_serial("Warning: Invalid TAR checksum, skipping\n");
            current_pos += 512;
            continue;
        }
        
        // Traite seulement les fichiers normaux
        if (header->typeflag == '0' || header->typeflag == '\0') {
            int file_size = oct2bin(header->size, 11);
            
            // Copie les informations du fichier
            strcpy(files_array[file_index].name, header->name);
            files_array[file_index].size = file_size;
            files_array[file_index].offset = current_pos + 512;
            files_array[file_index].data = (char*)(current_pos + 512);
            
            file_index++;
            
            // Calcule la position du prochain en-tête (aligné sur 512 octets)
            uint32_t data_blocks = (file_size + 511) / 512;
            current_pos += 512 + (data_blocks * 512);
        } else {
            // Ignore les autres types de fichiers (répertoires, liens, etc.)
            current_pos += 512;
        }
    }
    
    current_initrd->file_count = file_index;
    
    print_string_serial("Initrd initialized: ");
    // Conversion simple d'entier en chaîne
    char count_str[16];
    int temp = file_index;
    int i = 0;
    if (temp == 0) {
        count_str[i++] = '0';
    } else {
        while (temp > 0) {
            count_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    count_str[i] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = count_str[j];
        count_str[j] = count_str[i - 1 - j];
        count_str[i - 1 - j] = tmp;
    }
    
    print_string_serial(count_str);
    print_string_serial(" files found\n");
}

// Liste tous les fichiers dans l'initrd
void initrd_list_files() {
    if (!current_initrd) {
        print_string_serial("Initrd not initialized\n");
        return;
    }
    
    print_string_serial("Files in initrd:\n");
    for (uint32_t i = 0; i < current_initrd->file_count; i++) {
        print_string_serial("  ");
        print_string_serial(current_initrd->files[i].name);
        print_string_serial(" (");
        
        // Affiche la taille (conversion simple)
        uint32_t size = current_initrd->files[i].size;
        char size_str[16];
        int j = 0;
        if (size == 0) {
            size_str[j++] = '0';
        } else {
            while (size > 0) {
                size_str[j++] = '0' + (size % 10);
                size /= 10;
            }
        }
        size_str[j] = '\0';
        
        // Inverse la chaîne
        for (int k = 0; k < j / 2; k++) {
            char tmp = size_str[k];
            size_str[k] = size_str[j - 1 - k];
            size_str[j - 1 - k] = tmp;
        }
        
        print_string_serial(size_str);
        print_string_serial(" bytes)\n");
    }
}

// Lit le contenu d'un fichier
char* initrd_read_file(const char* filename) {
    if (!current_initrd) {
        return 0;
    }
    
    for (uint32_t i = 0; i < current_initrd->file_count; i++) {
        const char* stored_name = current_initrd->files[i].name;
        
        // Ignore le préfixe "./" si présent
        if (stored_name[0] == '.' && stored_name[1] == '/') {
            stored_name += 2;
        }
        
        if (strcmp(stored_name, filename) == 0) {
            return current_initrd->files[i].data;
        }
    }
    
    return 0; // Fichier non trouvé
}

// Obtient la taille d'un fichier
uint32_t initrd_get_file_size(const char* filename) {
    if (!current_initrd) {
        return 0;
    }
    
    for (uint32_t i = 0; i < current_initrd->file_count; i++) {
        const char* stored_name = current_initrd->files[i].name;
        
        // Ignore le préfixe "./" si présent
        if (stored_name[0] == '.' && stored_name[1] == '/') {
            stored_name += 2;
        }
        
        if (strcmp(stored_name, filename) == 0) {
            return current_initrd->files[i].size;
        }
    }
    
    return 0; // Fichier non trouvé
}

// Vérifie si un fichier existe
int initrd_file_exists(const char* filename) {
    return (initrd_read_file(filename) != 0);
}

// Obtient le nombre de fichiers
uint32_t initrd_get_file_count() {
    if (!current_initrd) {
        return 0;
    }
    return current_initrd->file_count;
}

// Affiche les informations d'un fichier
void initrd_print_file_info(const char* filename) {
    if (!current_initrd) {
        print_string_serial("Initrd not initialized\n");
        return;
    }
    
    for (uint32_t i = 0; i < current_initrd->file_count; i++) {
        if (strcmp(current_initrd->files[i].name, filename) == 0) {
            print_string_serial("File: ");
            print_string_serial(filename);
            print_string_serial("\nSize: ");
            
            // Affiche la taille
            uint32_t size = current_initrd->files[i].size;
            char size_str[16];
            int j = 0;
            if (size == 0) {
                size_str[j++] = '0';
            } else {
                while (size > 0) {
                    size_str[j++] = '0' + (size % 10);
                    size /= 10;
                }
            }
            size_str[j] = '\0';
            
            // Inverse la chaîne
            for (int k = 0; k < j / 2; k++) {
                char tmp = size_str[k];
                size_str[k] = size_str[j - 1 - k];
                size_str[j - 1 - k] = tmp;
            }
            
            print_string_serial(size_str);
            print_string_serial(" bytes\n");
            return;
        }
    }
    
    print_string_serial("File not found: ");
    print_string_serial(filename);
    print_string_serial("\n");
}

