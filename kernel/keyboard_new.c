// Fichier: kernel/keyboard_new.c

// Déclaration externe pour la fonction inb
unsigned char inb(unsigned short port);

// Déclaration externe pour la fonction qui ajoute un caractère au buffer système
void syscall_add_input_char(char c);

// Déclaration externe pour l'envoi de l'End-Of-Interrupt
void pic_send_eoi(unsigned char irq);

// Tableau de conversion scancode -> ASCII (layout US QWERTY simple)
unsigned char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};

// Le gestionnaire d'interruption pour le clavier
void keyboard_interrupt_handler() {
    unsigned char scancode = inb(0x60); // Lire le scancode

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            // Ajoute le caractère au buffer que le shell lira via un appel système
            syscall_add_input_char(c);
        }
    }

    // Envoyer l'acquittement (End-Of-Interrupt)
    pic_send_eoi(1);
}
