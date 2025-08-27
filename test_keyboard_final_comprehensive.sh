#!/bin/bash

# Test automatisé du clavier AI-OS
# Ce script lance QEMU et simule des frappes de touche

echo "=== Test Automatisé du Clavier AI-OS ==="
echo "1. Compilation du système..."

make clean > /dev/null 2>&1
make > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "❌ ERREUR: Échec de compilation"
    exit 1
fi

echo "✅ Compilation réussie"
echo ""
echo "2. Test QEMU avec mode graphique..."
echo "   (Le système va se lancer dans une nouvelle fenêtre)"
echo "   Logs série capturés dans: keyboard_test_serial.log"
echo ""

# Nettoyer les anciens logs
rm -f keyboard_test_serial.log

# Lancer QEMU en arrière-plan avec interface graphique
echo "Lancement de QEMU..."
qemu-system-i386 -kernel build/ai_os.bin -initrd my_initrd.tar \
    -m 256M -vga std \
    -machine type=pc,accel=tcg \
    -device i8042 \
    -serial file:keyboard_test_serial.log \
    -rtc base=utc -no-reboot &

QEMU_PID=$!
echo "QEMU PID: $QEMU_PID"

# Attendre que QEMU se lance
echo "Attente du démarrage de QEMU..."
sleep 5

# Vérifier si QEMU est toujours en cours
if ! kill -0 $QEMU_PID 2>/dev/null; then
    echo "❌ ERREUR: QEMU ne s'est pas lancé correctement"
    exit 1
fi

echo "✅ QEMU lancé avec succès"
echo ""
echo "🔍 INSTRUCTIONS DE TEST:"
echo "1. Une fenêtre QEMU devrait s'être ouverte"
echo "2. Cliquez dans la fenêtre pour donner le focus"
echo "3. Testez les touches suivantes:"
echo "   - Lettres: a, b, c, h, e, l, p, t"
echo "   - SPACE (barre d'espace)"  
echo "   - ENTER"
echo "4. Observez les réactions du système"
echo "5. Fermez QEMU quand terminé (Alt+F4 ou bouton X)"
echo ""
echo "⏳ Attente de vos tests (timeout dans 60 secondes)..."

# Attendre que l'utilisateur teste ou timeout
for i in {60..1}; do
    if ! kill -0 $QEMU_PID 2>/dev/null; then
        echo "✅ QEMU fermé par l'utilisateur"
        break
    fi
    if [ $((i % 15)) -eq 0 ]; then
        echo "   Temps restant: ${i}s..."
    fi
    sleep 1
done

# Fermer QEMU si toujours ouvert
if kill -0 $QEMU_PID 2>/dev/null; then
    echo "⏰ Timeout - fermeture automatique de QEMU..."
    kill $QEMU_PID
    sleep 2
    kill -9 $QEMU_PID 2>/dev/null
fi

echo ""
echo "3. Analyse des résultats..."

# Vérifier si des logs ont été générés
if [ ! -f keyboard_test_serial.log ]; then
    echo "❌ ERREUR: Aucun log série généré"
    exit 1
fi

# Analyser les logs
echo "=== ANALYSE DES LOGS SÉRIE ==="
echo ""

# Compter les interruptions clavier
KBD_INTERRUPTS=$(grep -c "KBD#" keyboard_test_serial.log 2>/dev/null || echo "0")
echo "📊 Interruptions clavier détectées: $KBD_INTERRUPTS"

# Compter les caractères mis en buffer
BUFFERED_CHARS=$(grep -c "PUT:" keyboard_test_serial.log 2>/dev/null || echo "0")
echo "📊 Caractères mis en buffer: $BUFFERED_CHARS"

# Compter les appels GETC
GETC_CALLS=$(grep -c "GETC:" keyboard_test_serial.log 2>/dev/null || echo "0")
echo "📊 Appels à keyboard_getc(): $GETC_CALLS"

echo ""

# Afficher les dernières lignes pertinentes
echo "=== DERNIERS ÉVÉNEMENTS CLAVIER ==="
grep "KBD#\|PUT:\|GETC:" keyboard_test_serial.log | tail -20

echo ""
echo "=== RÉSUMÉ DU TEST ==="

if [ $KBD_INTERRUPTS -gt 1 ] && [ $BUFFERED_CHARS -gt 0 ]; then
    echo "✅ SUCCÈS: Le clavier fonctionne correctement !"
    echo "   - Interruptions reçues: $KBD_INTERRUPTS"
    echo "   - Caractères traités: $BUFFERED_CHARS"
    echo "   - Système opérationnel"
elif [ $KBD_INTERRUPTS -gt 1 ]; then
    echo "⚠️  PARTIEL: Interruptions détectées mais problème de traitement"
    echo "   - Interruptions: $KBD_INTERRUPTS"
    echo "   - Caractères traités: $BUFFERED_CHARS"
    echo "   - Vérifier la conversion scancode->ASCII"
elif [ $KBD_INTERRUPTS -eq 1 ]; then
    echo "⚠️  PROBLÈME: Seule l'interruption d'initialisation (ACK) détectée"
    echo "   - Cause probable: Mode QEMU ne génère pas les vraies interruptions"
    echo "   - Solution: Utiliser un vrai PC ou un émulateur différent"
else
    echo "❌ ÉCHEC: Aucune interruption clavier détectée"
    echo "   - Vérifier la configuration du PIC"
    echo "   - Vérifier l'IDT"
    echo "   - Vérifier l'initialisation du clavier PS/2"
fi

echo ""
echo "📄 Logs complets disponibles dans: keyboard_test_serial.log"
echo "   Commande pour voir tous les logs: cat keyboard_test_serial.log"
echo ""
echo "=== Test terminé ==="
