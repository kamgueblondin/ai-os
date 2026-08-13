/* idle.c — tâche userspace de fond. Reste en file jusqu'à kill.
 * Ne pas la lancer via exec (bloquant). spawn ne change pas de contexte :
 * le shell garde le CPU, idle reste TASK_READY. */

void yield(void) {
    asm volatile("int $0x80" : : "a"(4));
}

void main(void) {
    for (;;) {
        yield();
    }
}
