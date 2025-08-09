# TODO - Implémentation de la gestion des interruptions et du clavier

## Phase 3: Implémentation de la gestion des interruptions et du clavier

### Fichiers à créer:
- [x] kernel/idt.h - Définitions pour l'IDT
- [x] kernel/idt.c - Logique de création de l'IDT  
- [x] boot/idt_loader.s - Code assembleur pour charger l'IDT
- [x] kernel/interrupts.h - Définitions pour les interruptions
- [x] kernel/interrupts.c - Gestion du PIC et des ISR
- [x] kernel/keyboard.h - Définitions pour le clavier
- [x] kernel/keyboard.c - Pilote du clavier

### Modifications à apporter:
- [x] Modifier kernel/kernel.c pour intégrer les nouvelles fonctionnalités
- [x] Mettre à jour le Makefile pour inclure les nouveaux fichiers
- [x] Créer les stubs ISR en assembleur (isr_stubs.s)

### Fonctionnalités à implémenter:
- [x] IDT (Interrupt Descriptor Table) avec 256 entrées
- [x] Remappage du PIC (Programmable Interrupt Controller)
- [x] Gestion des interruptions clavier (IRQ 1)
- [x] Traduction des scancodes en caractères ASCII
- [x] Affichage des caractères tapés à l'écran

### Tests à effectuer:
- [x] Compilation sans erreurs
- [x] Démarrage correct dans QEMU
- [x] Réponse aux touches du clavier
- [x] Affichage correct des caractères

