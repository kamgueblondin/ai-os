#!/bin/bash

echo "🔬 TEST EN TEMPS RÉEL - MODE DEBUG COMPLET"
echo "=========================================="
echo "Date: $(date)"
echo "Objectif: Identifier l'exact point de blocage du clavier"
echo ""

cd /workspace/ai-os

# Créer un fichier de log pour capturer tous les debug
LOG_FILE="debug_temps_reel_$(date +%s).log"

echo "📋 INSTRUCTIONS POUR LE TEST:"
echo "1. QEMU va démarrer avec logging série complet"
echo "2. Attendre que le shell AI-OS s'affiche"
echo "3. Essayer de taper des caractères (a, b, c, ENTER)"
echo "4. Observer les messages debug en temps réel"
echo "5. Utiliser Ctrl+A puis X pour quitter QEMU"
echo ""
echo "🔍 ANALYSE QUI SERA EFFECTUÉE:"
echo "- IRQ1_START: Interruptions clavier reçues ?"
echo "- IRQ1_SCAN: Scancodes détectés ?"
echo "- BUFFER_PUT: Caractères mis dans le buffer ?"
echo "- GETC_START: Appels à keyboard_getc() ?"
echo "- BUFFER_GET: Lecture du buffer ?"
echo "- GETC_SUCCESS: Retour de caractères ?"
echo ""
echo "📝 LOG: Tous les debug seront sauvés dans: $LOG_FILE"
echo ""

# Fonction pour analyser les logs après le test
analyze_log() {
    if [ -f "$LOG_FILE" ]; then
        echo ""
        echo "📊 ANALYSE AUTOMATIQUE DU LOG:"
        echo "=============================="
        
        echo "🔌 Interruptions IRQ1:"
        grep -c "IRQ1_START" "$LOG_FILE" || echo "0"
        
        echo "📨 Scancodes reçus:"
        grep -c "IRQ1_SCAN" "$LOG_FILE" || echo "0"
        
        echo "📥 Caractères mis en buffer:"
        grep -c "BUFFER_PUT" "$LOG_FILE" || echo "0"
        
        echo "🔍 Appels à keyboard_getc():"
        grep -c "GETC_START" "$LOG_FILE" || echo "0"
        
        echo "📤 Lectures du buffer:"
        grep -c "BUFFER_GET" "$LOG_FILE" || echo "0"
        
        echo "✅ Succès de lecture:"
        grep -c "GETC_SUCCESS" "$LOG_FILE" || echo "0"
        
        echo "⏱️ Timeouts:"
        grep -c "GETC_TIMEOUT" "$LOG_FILE" || echo "0"
        
        echo ""
        echo "🎯 DIAGNOSTIC AUTOMATIQUE:"
        
        # Analyser le pattern de problème
        irq_count=$(grep -c "IRQ1_START" "$LOG_FILE" 2>/dev/null || echo "0")
        getc_count=$(grep -c "GETC_START" "$LOG_FILE" 2>/dev/null || echo "0")
        buffer_puts=$(grep -c "BUFFER_PUT" "$LOG_FILE" 2>/dev/null || echo "0")
        
        if [ "$irq_count" -eq "0" ]; then
            echo "❌ PROBLÈME: Aucune interruption IRQ1 détectée"
            echo "   → Le contrôleur PS/2 ne génère pas d'interruptions"
            echo "   → Vérifier l'initialisation PS/2 et la configuration PIC"
        elif [ "$buffer_puts" -eq "0" ] && [ "$irq_count" -gt "0" ]; then
            echo "❌ PROBLÈME: Interruptions reçues mais aucun caractère mis en buffer"
            echo "   → Problème de conversion scancode→ASCII"
            echo "   → Ou scancodes ignorés (key release, etc.)"
        elif [ "$getc_count" -eq "0" ]; then
            echo "❌ PROBLÈME: keyboard_getc() jamais appelé"
            echo "   → Problème dans le shell ou les syscalls"
        else
            echo "✅ Chaîne fonctionnelle détectée - analyser les détails"
        fi
        
        echo ""
        echo "📄 Log complet disponible dans: $LOG_FILE"
        echo "Pour analyse détaillée: less $LOG_FILE"
    fi
}

# Trap pour analyser les logs à la sortie
trap analyze_log EXIT

echo "🚀 LANCEMENT DU TEST QEMU..."
echo "Appuyez sur ENTER pour continuer..."
read -r

# Lancer QEMU avec redirection du port série vers le log
echo "Starting QEMU with debug logging..."
timeout 120 qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 256M \
    -serial file:$LOG_FILE \
    -monitor stdio \
    -display gtk,grab-on-hover=on 2>&1

echo ""
echo "🏁 TEST TERMINÉ"
