#!/bin/bash

# AI-OS Keyboard Fix - Ultimate Solution
# Tests multiples configurations QEMU pour résoudre le problème clavier

set -e

echo "=== AI-OS KEYBOARD FIX - SOLUTION ULTIMATE ==="
echo "Diagnostic: Problème clavier persistant avec configuration actuelle"
echo "Solution: Tests multiples configurations QEMU + Driver hybride"
echo ""

# Nettoyer l'environnement
echo "🧹 Nettoyage complet..."
rm -rf build
rm -f my_initrd.tar
rm -rf initrd_content
rm -f *.log
make clean

# Appliquer la correction ultimate
echo "🔧 Application du driver clavier ultimate..."
if [ -f "kernel/keyboard_ultimate.c" ]; then
    echo "   ✓ Installation du driver hybride (interruption + polling)"
    cp kernel/keyboard_ultimate.c kernel/keyboard.c
else
    echo "   ❌ ERREUR: kernel/keyboard_ultimate.c non trouvé!"
    exit 1
fi

# Compilation
echo "🔨 Compilation AI-OS..."
make
if [ $? -ne 0 ]; then
    echo "❌ ERREUR DE COMPILATION"
    exit 1
fi

echo ""
echo "✅ COMPILATION RÉUSSIE"
echo "Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

# Function pour tester une configuration QEMU
test_qemu_config() {
    local config_name="$1"
    local timeout_sec="$2"
    shift 2
    local qemu_args=("$@")
    
    echo "🚀 TEST: $config_name"
    echo "   Paramètres: ${qemu_args[*]}"
    echo "   Timeout: ${timeout_sec}s"
    echo ""
    
    echo "📝 INSTRUCTIONS:"
    echo "   1. Attendez le prompt shell AI-OS"
    echo "   2. Tapez quelques lettres (a, b, c)"  
    echo "   3. Si ça marche: tapez 'help' puis fermez"
    echo "   4. Si ça ne marche pas: fermez après quelques secondes"
    echo ""
    
    # Lancer QEMU avec timeout
    timeout "${timeout_sec}s" qemu-system-i386 "${qemu_args[@]}" || true
    
    echo ""
    echo "Test $config_name terminé."
    echo ""
}

# Configuration 1: Standard PC avec périphériques PS/2 explicites
echo "=== CONFIGURATION 1: PC Standard + PS/2 Explicite ==="
test_qemu_config "PC-Standard-PS2" 30 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu pentium3 \
    -device i8042 \
    -device ps2-kbd \
    -vga std \
    -display gtk,zoom-to-fit=on \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown

echo "Appuyez sur Entrée pour passer au test suivant..."
read -r

# Configuration 2: Machine QEMU générique
echo "=== CONFIGURATION 2: Machine QEMU Générique ==="  
test_qemu_config "QEMU-Generic" 30 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine q35 \
    -cpu qemu32 \
    -vga std \
    -display gtk \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown

echo "Appuyez sur Entrée pour passer au test suivant..."
read -r

# Configuration 3: Mode VNC (pas d'interface directe)
echo "=== CONFIGURATION 3: Mode VNC ==="
echo "Ouvrez un client VNC sur localhost:5900 après le lancement"
test_qemu_config "VNC-Mode" 45 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu pentium \
    -vga std \
    -vnc :0 \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown

echo "Appuyez sur Entrée pour passer au test suivant..."
read -r

# Configuration 4: Mode SDL (alternative à GTK)
echo "=== CONFIGURATION 4: Mode SDL ==="
test_qemu_config "SDL-Mode" 30 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu i386 \
    -vga std \
    -display sdl \
    -chardev stdio,id=serial0 \
    -device isa-serial,chardev=serial0 \
    -no-reboot -no-shutdown

echo "Appuyez sur Entrée pour passer au test suivant..."
read -r

# Configuration 5: Mode console série pur (sans GUI)
echo "=== CONFIGURATION 5: Console Série Pure ==="
echo "ATTENTION: Aucune interface graphique - interaction par série uniquement"
test_qemu_config "Serial-Console" 30 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -machine pc \
    -cpu i486 \
    -nographic \
    -serial stdio \
    -no-reboot -no-shutdown

echo "Appuyez sur Entrée pour continuer..."
read -r

# Configuration 6: Émulation ultra-simple
echo "=== CONFIGURATION 6: Émulation Ultra-Simple ==="
test_qemu_config "Ultra-Simple" 30 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 64M \
    -display gtk \
    -no-reboot -no-shutdown

echo ""
echo "=== TESTS TERMINÉS ==="
echo ""
echo "📋 RÉSULTATS:"
echo "Notez quelle(s) configuration(s) ont permis la saisie clavier:"
echo "  [ ] Configuration 1: PC Standard + PS/2 Explicite"
echo "  [ ] Configuration 2: Machine QEMU Générique"  
echo "  [ ] Configuration 3: Mode VNC"
echo "  [ ] Configuration 4: Mode SDL"
echo "  [ ] Configuration 5: Console Série Pure"
echo "  [ ] Configuration 6: Émulation Ultra-Simple"
echo ""
echo "💡 NEXT STEPS:"
echo "1. Identifiez la configuration qui marche"
echo "2. Créez un script run.sh avec cette configuration"
echo "3. Testez avec des programmes utilisateur complexes"
echo ""
echo "🔍 LOGS DISPONIBLES:"
echo "   - Sortie console dans ce terminal"
echo "   - Logs QEMU dans les fichiers *.log générés"
