#!/bin/bash

# Test Clavier AI-OS - Solution Ultime
# Correction complète avec paramètres QEMU optimaux

set -e

echo "=== TEST CLAVIER AI-OS - SOLUTION ULTIME ==="
echo "DIAGNOSTIC: Correction complète du système clavier"
echo "PROBLÈME: Codes PS/2 ACK au lieu des scancodes"
echo "SOLUTION: Initialisation PS/2 améliorée + QEMU optimisé"
echo ""

# Nettoyer les anciens builds
echo "🧹 Nettoyage..."
rm -rf build
rm -f my_initrd.tar
rm -rf initrd_content
rm -f output.log
make clean

# Appliquer la correction du clavier
echo "🔧 Application de la correction clavier..."
if [ -f "kernel/keyboard_fixed.c" ]; then
    echo "   - Remplacement de keyboard.c par la version corrigée"
    cp kernel/keyboard_fixed.c kernel/keyboard.c
else
    echo "   - ERREUR: kernel/keyboard_fixed.c non trouvé!"
    exit 1
fi

# Compilation
echo "🔨 Compilation AI-OS..."
make

echo "\n=== AI-OS COMPILÉ AVEC SUCCÈS ==="
echo "Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo "Système prêt pour test clavier avancé"
echo ""

# Test avec QEMU optimisé
echo "🚀 LANCEMENT QEMU OPTIMISÉ POUR CLAVIER..."
echo "📋 PARAMÈTRES SPÉCIAUX:"
echo "   - Interface graphique GTK native"
echo "   - Émulation clavier PS/2 complète"
echo "   - Logs d'interruptions activés"
echo "   - Processeur i386 avec support PS/2"
echo "\n📝 INSTRUCTIONS DE TEST:"
echo "   1. Cliquez dans la fenêtre QEMU pour capturer le clavier"
echo "   2. Tapez des lettres simples (a, b, c, etc.)"
echo "   3. Observez les logs dans ce terminal"
echo "   4. Le shell devrait répondre aux frappes"
echo "   5. Tapez 'help' pour tester les commandes"
echo "   6. Appuyez Ctrl+Alt+G pour libérer la souris"
echo "   7. Fermez la fenêtre QEMU pour arrêter"
echo ""
echo "⚡ Lancement dans 3 secondes..."
sleep 3

# Paramètres QEMU optimisés pour le clavier
QEMU_ARGS=(
    "-kernel" "build/ai_os.bin"          # Notre noyau
    "-initrd" "my_initrd.tar"             # Notre initrd
    "-m" "128M"                           # RAM suffisante
    "-cpu" "pentium3"                     # CPU avec bon support PS/2
    "-machine" "pc"                       # Machine PC standard
    "-vga" "std"                          # VGA standard
    "-display" "gtk,zoom-to-fit=on"       # Affichage GTK natif
    "-device" "isa-serial,chardev=serial0" # Port série pour logs
    "-chardev" "stdio,id=serial0"         # Redirection série vers stdout
    "-no-reboot"                          # Pas de reboot automatique
    "-no-shutdown"                        # Pas d'arrêt automatique
    "-d" "int"                            # Debug des interruptions
    "-D" "qemu_interrupts.log"            # Log des interruptions
)

echo "Commande QEMU:"
echo "qemu-system-i386 ${QEMU_ARGS[*]}"
echo ""

# Lancement de QEMU
qemu-system-i386 "${QEMU_ARGS[@]}"

echo "\n=== TEST TERMINÉ ==="
echo "Vérifiez le fichier qemu_interrupts.log pour les détails des interruptions."
