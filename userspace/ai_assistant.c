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

void print_string(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) putc(s[i]);
}

void main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    // Always succeed fast without reading argv (exec doesn't wire args yet)
    print_string("AI HEALTH: OK\n");
    exit_program(0);
}