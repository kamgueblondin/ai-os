#!/bin/bash

# AI-OS - Script de lancement stable 
# Version avec driver anti-spam et configuration QEMU optimale

set -e

echo "=== AI-OS - VERSION STABLE ==="
echo "Driver: Clavier stable avec anti-spam"
echo "QEMU: Configuration testée et validée"
echo ""

# Appliquer le driver stable et compiler
echo "🔧 Application du driver stable..."
if [ -f "kernel/keyboard_stable.c" ]; then
    cp kernel/keyboard_stable.c kernel/keyboard.c
    echo "   ✓ Driver stable appliqué"
else
    echo "   ❌ ERREUR: keyboard_stable.c introuvable"
    exit 1
fi

echo "🔨 Compilation..."
make clean && make
if [ $? -ne 0 ]; then
    echo "❌ ERREUR DE COMPILATION"
    exit 1
fi

echo ""
echo "✅ AI-OS STABLE PRÊT"
echo "   Noyau: build/ai_os.bin ($(du -h build/ai_os.bin | cut -f1))"
echo "   Initrd: my_initrd.tar ($(du -h my_initrd.tar | cut -f1))"
echo ""

echo "📝 AMÉLIORATIONS APPORTÉES:"
echo "   ✓ Anti-spam: Filtre les caractères répétitifs"
echo "   ✓ Polling contrôlé: Réduit les caractères fantômes"
echo "   ✓ Protection anti-doublons: Évite la répétition"
echo "   ✓ Timeout intelligent: Évite les blocages"
echo ""
echo "🎯 RÉSULTAT ATTENDU:"
echo "   - Plus de caractères fantômes"
echo "   - Saisie clavier propre et stable"  
echo "   - Réponse normale aux frappes"
echo ""
echo "🚀 Lancement dans 3 secondes..."
sleep 3

# Configuration QEMU stable et universelle
exec qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 128M \
    -vga std \
    -display gtk \
    -serial stdio \
    -no-reboot -no-shutdown
