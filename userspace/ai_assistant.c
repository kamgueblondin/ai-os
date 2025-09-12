// ai_assistant.c - Minimal assistant to answer healthcheck quickly (ASCII-only)

void putc(char c) {
    asm volatile("int $0x80" : : "a"(1), "b"(c));
}

void exit_program(int code) {
    asm volatile("int $0x80" : : "a"(0), "b"(code));
}

int strlen(const char* s) {
    int n = 0; while (s[n] != '\0') n++; return n;
}

int strcmp(const char* a, const char* b) {
    int i = 0; while (a[i] && b[i] && a[i] == b[i]) i++; return (unsigned char)a[i] - (unsigned char)b[i];
}

char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') return (char*)haystack;

    for (int i = 0; haystack[i] != '\0'; i++) {
        int j = 0;
        while (haystack[i + j] == needle[j] && needle[j] != '\0') {
            j++;
        }
        if (needle[j] == '\0') {
            return (char*)&haystack[i];
        }
    }
    return 0; // NULL
}

void to_lowercase(char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = str[i] + 32;
        }
    }
}

void print_string(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) putc(s[i]);
}

void main(int argc, char* argv[]) {
    print_string("[AI] start\n");
    const char* q = 0;
    if (argc >= 2 && argv[1]) q = argv[1];

    if (q && q[0] != '\0') {
        char query_lower[256];
        int i = 0;
        while (q[i] != '\0' && i < 255) {
            query_lower[i] = q[i];
            i++;
        }
        query_lower[i] = '\0';
        to_lowercase(query_lower);

        if (strcmp(query_lower, "healthcheck") == 0) {
            print_string("AI HEALTH: OK\n");
            print_string("[AI] end\n");
            exit_program(0);
        }

        int has_bonjour = strstr(query_lower, "bonjour") != 0;
        int has_hello = strstr(query_lower, "hello") != 0;

        if (has_bonjour || has_hello) {
            print_string("AI: bonjour\n");
            print_string("[AI] end\n");
            exit_program(0);
        }

        print_string("AI: received\n");
        print_string("[AI] end\n");
        exit_program(0);
    }

    print_string("AI: received\n");
    print_string("[AI] end\n");
    exit_program(0);
}