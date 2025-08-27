#!/bin/bash

echo "🔬 TEST DEBUG AUTOMATISÉ - AI-OS CLAVIER"
echo "========================================"
echo "Date: $(date)"
echo ""

cd /workspace/ai-os

# Créer un fichier de log unique
LOG_FILE="debug_auto_$(date +%s).log"

echo "📋 LANCEMENT AUTOMATIQUE DU TEST:"
echo "- QEMU lancé en mode headless avec timeout 30s"
echo "- Capture complète des logs série debug"
echo "- Analyse automatique des résultats"
echo ""
echo "📝 LOG: $LOG_FILE"
echo ""

# Fonction d'analyse complète
analyze_complete_log() {
    if [ -f "$LOG_FILE" ]; then
        echo ""
        echo "📊 ANALYSE COMPLÈTE DU DEBUG LOG"
        echo "================================="
        
        # Statistiques globales
        echo ""
        echo "🔢 STATISTIQUES GLOBALES:"
        total_lines=$(wc -l < "$LOG_FILE" 2>/dev/null || echo "0")
        echo "Total lignes debug: $total_lines"
        
        # Analyse des interruptions
        echo ""
        echo "🔌 INTERRUPTIONS IRQ1:"
        irq_starts=$(grep -c "IRQ1_START" "$LOG_FILE" 2>/dev/null || echo "0")
        irq_scans=$(grep -c "IRQ1_SCAN" "$LOG_FILE" 2>/dev/null || echo "0")
        irq_puts=$(grep -c "IRQ1_PUT" "$LOG_FILE" 2>/dev/null || echo "0")
        
        echo "- IRQ1_START (interruptions reçues): $irq_starts"
        echo "- IRQ1_SCAN (scancodes lus): $irq_scans"  
        echo "- IRQ1_PUT (caractères convertis): $irq_puts"
        
        # Analyse du buffer
        echo ""
        echo "📦 BUFFER CIRCULAIRE:"
        buffer_puts=$(grep -c "BUFFER_PUT" "$LOG_FILE" 2>/dev/null || echo "0")
        buffer_gets=$(grep -c "BUFFER_GET" "$LOG_FILE" 2>/dev/null || echo "0")
        buffer_empty=$(grep -c "BUFFER_EMPTY" "$LOG_FILE" 2>/dev/null || echo "0")
        
        echo "- BUFFER_PUT (caractères ajoutés): $buffer_puts"
        echo "- BUFFER_GET (caractères lus): $buffer_gets"
        echo "- BUFFER_EMPTY (buffer vide): $buffer_empty"
        
        # Analyse des appels getc
        echo ""
        echo "🔍 KEYBOARD_GETC():"
        getc_starts=$(grep -c "GETC_START" "$LOG_FILE" 2>/dev/null || echo "0")
        getc_success=$(grep -c "GETC_SUCCESS" "$LOG_FILE" 2>/dev/null || echo "0")
        getc_timeout=$(grep -c "GETC_TIMEOUT" "$LOG_FILE" 2>/dev/null || echo "0")
        getc_waits=$(grep -c "GETC_WAIT" "$LOG_FILE" 2>/dev/null || echo "0")
        
        echo "- GETC_START (appels): $getc_starts"
        echo "- GETC_SUCCESS (succès): $getc_success"
        echo "- GETC_TIMEOUT (timeouts): $getc_timeout"
        echo "- GETC_WAIT (cycles attente): $getc_waits"
        
        # Diagnostic automatique détaillé
        echo ""
        echo "🎯 DIAGNOSTIC AUTOMATIQUE DÉTAILLÉ:"
        echo "====================================="
        
        if [ "$total_lines" -lt "50" ]; then
            echo "❌ PROBLÈME CRITIQUE: Très peu de debug généré ($total_lines lignes)"
            echo "   → Le système ne démarre pas correctement"
            echo "   → Vérifier la compilation ou le boot du kernel"
            
        elif [ "$irq_starts" -eq "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: AUCUNE INTERRUPTION IRQ1"
            echo "   → Le contrôleur PS/2 ne génère pas d'interruptions"
            echo "   → Solutions possibles:"
            echo "     1. Vérifier la configuration PIC (masque IRQ1)"
            echo "     2. Vérifier l'initialisation PS/2"
            echo "     3. Problème d'émulation QEMU"
            
        elif [ "$irq_scans" -eq "0" ] && [ "$irq_starts" -gt "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: INTERRUPTIONS REÇUES MAIS AUCUN SCANCODE"
            echo "   → Handler IRQ1 appelé mais pas de lecture du port 0x60"
            echo "   → Problème dans keyboard_interrupt_handler()"
            
        elif [ "$buffer_puts" -eq "0" ] && [ "$irq_scans" -gt "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: SCANCODES LUES MAIS AUCUN CARACTÈRE EN BUFFER"
            echo "   → Problème de conversion scancode → ASCII"
            echo "   → Ou tous les scancodes sont ignorés (key releases, etc.)"
            echo "   → Vérifier scancode_to_ascii() et la table de conversion"
            
        elif [ "$getc_starts" -eq "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: KEYBOARD_GETC() JAMAIS APPELÉ"
            echo "   → Le shell n'appelle pas sys_getchar()"
            echo "   → Ou problème dans les syscalls"
            echo "   → Vérifier la boucle principale du shell"
            
        elif [ "$buffer_gets" -eq "0" ] && [ "$getc_starts" -gt "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: GETC APPELÉ MAIS BUFFER JAMAIS LU"
            echo "   → Problème dans kbd_get_nonblock()"
            echo "   → Buffer probablement toujours vide"
            
        elif [ "$getc_success" -eq "0" ] && [ "$buffer_gets" -gt "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: BUFFER LU MAIS JAMAIS DE SUCCÈS"
            echo "   → Tous les appels retournent buffer vide"
            echo "   → Problème de synchronisation head/tail"
            
        elif [ "$getc_timeout" -gt "0" ] && [ "$getc_success" -eq "0" ]; then
            echo "❌ PROBLÈME IDENTIFIÉ: TIMEOUTS RÉPÉTÉS"
            echo "   → keyboard_getc() n'arrive jamais à lire un caractère"
            echo "   → Buffer reste vide malgré les interruptions"
            
        else
            echo "✅ CHAÎNE FONCTIONNELLE DÉTECTÉE"
            echo "   → IRQ1: $irq_starts, Buffer: $buffer_puts, GETC: $getc_success"
            echo "   → Le problème est plus subtil - analyser les détails"
        fi
        
        # Échantillons de debug
        echo ""
        echo "📄 ÉCHANTILLONS DU LOG:"
        echo "======================="
        
        echo ""
        echo "🔌 Premières interruptions:"
        head -20 "$LOG_FILE" | grep -E "(IRQ1|INIT)" || echo "Aucune trouvée"
        
        echo ""
        echo "🔍 Premiers appels GETC:"
        grep "GETC_START" "$LOG_FILE" | head -5 || echo "Aucun trouvé"
        
        echo ""
        echo "📦 État du buffer:"
        grep -E "(BUFFER_PUT|BUFFER_GET|BUFFER_EMPTY)" "$LOG_FILE" | head -5 || echo "Aucun trouvé"
        
        echo ""
        echo "⏱️ Événements timeout/wait:"
        grep -E "(TIMEOUT|WAIT)" "$LOG_FILE" | head -3 || echo "Aucun trouvé"
        
    else
        echo "❌ ERREUR: Fichier log '$LOG_FILE' introuvable"
        echo "Le test QEMU a probablement échoué"
    fi
}

# Trap pour analyser les logs automatiquement
trap analyze_complete_log EXIT

echo "🚀 LANCEMENT QEMU AUTOMATIQUE (30s)..."

# Lancer QEMU en mode automatique avec timeout
timeout 30s qemu-system-i386 \
    -kernel build/ai_os.bin \
    -initrd my_initrd.tar \
    -m 256M \
    -serial file:$LOG_FILE \
    -display none \
    -no-reboot \
    >/dev/null 2>&1 &

QEMU_PID=$!

echo "QEMU lancé (PID: $QEMU_PID) - Attente des logs debug..."

# Attendre que le log commence à se remplir
sleep 5

# Montrer le log en temps réel
echo ""
echo "📺 LOGS DEBUG EN TEMPS RÉEL:"
echo "=============================="

# Surveiller le log pendant 20 secondes max
timeout 20s tail -f "$LOG_FILE" 2>/dev/null &
TAIL_PID=$!

# Attendre la fin
wait $QEMU_PID 2>/dev/null
kill $TAIL_PID 2>/dev/null

echo ""
echo "🏁 TEST AUTOMATIQUE TERMINÉ"
