/* wait_child.c - enfant de preuve pour SYS_TASK_WAIT.
 * Après un unique yield, le retour de main provoque SYS_EXIT via start.s. */

void putc(char c) {
    asm volatile("int $0x80" : : "a"(1), "b"(c));
}

void yield(void) {
    asm volatile("int $0x80" : : "a"(4));
}

static void puts_local(const char* text) {
    int i = 0;
    while (text[i]) putc(text[i++]);
}

void main(void) {
    puts_local("wait-child ready\n");
    yield();
    puts_local("wait-child done\n");
}
