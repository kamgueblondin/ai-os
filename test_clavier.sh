#!/bin/bash

echo "=== Test du Clavier AI-OS ==="
echo "Compilation du projet..."

# Compiler le projet
make clean > /dev/null 2>&1
if make > /dev/null 2>&1; then
    echo "✓ Compilation réussie"
else
    echo "✗ Erreur de compilation"
    exit 1
fi

echo "Lancement du test QEMU (30 secondes)..."

# Lancer QEMU avec timeout et capturer la sortie
timeout 30s qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -serial file:test_output.log \
    -display none \
    -no-reboot \
    -no-shutdown > /dev/null 2>&1

echo "Test terminé. Analyse des logs..."

# Vérifier les logs
if [ -f test_output.log ]; then
    echo "=== Résultats du Test ==="
    
    # Vérifier l'initialisation du clavier
    if grep -q "PS/2 Keyboard initialise" test_output.log; then
        echo "✓ Clavier PS/2 initialisé"
    else
        echo "✗ Problème d'initialisation du clavier"
    fi
    
    # Vérifier l'initialisation des syscalls
    if grep -q "Appels systeme initialises" test_output.log; then
        echo "✓ Syscalls initialisés"
    else
        echo "✗ Problème d'initialisation des syscalls"
    fi
    
    # Vérifier le démarrage du shell
    if grep -q "Shell trouve" test_output.log; then
        echo "✓ Shell trouvé et chargé"
    else
        echo "✗ Problème de chargement du shell"
    fi
    
    # Vérifier le timer
    if grep -q "Timer tick" test_output.log; then
        echo "✓ Timer fonctionnel"
    else
        echo "✗ Problème de timer"
    fi
    
    echo ""
    echo "Logs complets disponibles dans test_output.log"
    echo "Dernières lignes du log :"
    tail -10 test_output.log
else
    echo "✗ Aucun log généré"
fi

echo ""
echo "=== Instructions pour Test Manuel ==="
echo "Pour tester manuellement le clavier :"
echo "1. make run"
echo "2. Attendre l'affichage du prompt du shell"
echo "3. Taper des caractères et vérifier qu'ils s'affichent"
echo "4. Taper 'help' puis Entrée pour tester une commande"

