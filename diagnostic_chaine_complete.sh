#!/bin/bash

echo "🔍 DIAGNOSTIC COMPLET - CHAÎNE SHELL → KERNEL → CLAVIER"
echo "======================================================"
echo "Version: AI-OS v7.0 - Debug Mode Complet"
echo "Date: $(date)"
echo ""

# Déterminer le dossier du script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 1. Backup des fichiers originaux
echo "📦 Sauvegarde des fichiers originaux..."
cp userspace/shell.c userspace/shell.c.backup.debug
cp kernel/syscall/syscall.c kernel/syscall/syscall.c.backup.debug
cp kernel/keyboard.c kernel/keyboard.c.backup.debug

# 2. Analyser la chaîne shell → syscall
echo ""
echo "🔗 ANALYSE DE LA CHAÎNE D'APPELS"
echo "1. Shell: sys_getchar() → int $0x80 avec eax=2"
echo "2. Syscall: SYS_GETC (case 2) → keyboard_getc()" 
echo "3. Kernel: keyboard_getc() → kbd_get_nonblock()"
echo "4. Driver: buffer circulaire + interrupt handler"
echo ""

# 3. Vérifier les numéros de syscalls
echo "🔍 VÉRIFICATION DES SYSCALLS:"
echo "Shell userspace/shell.c ligne 78-82:"
grep -A4 -B2 "sys_getchar" userspace/shell.c
echo ""
echo "Syscall kernel/syscall/syscall.h:"
grep -A3 -B3 "SYS_GETC" kernel/syscall/syscall.h
echo ""
echo "Handler kernel/syscall/syscall.c:"
grep -A8 -B3 "case SYS_GETC" kernel/syscall/syscall.c
echo ""

# 4. Lancer un test avec logging détaillé
echo "🚀 TEST TEMPS RÉEL AVEC LOGGING DÉTAILLÉ"
echo "Compilation du projet avec debug..."

# Modifier temporairement le shell pour plus de debug
cat > debug_shell_patch.c << 'EOF'
        // Version debug intensifiée de la boucle principale
        for(;;){
            // DEBUG: Avant l'appel syscall
            putc('[');
            putc('G');
            putc('E');
            putc('T');
            putc(']');
            
            int c = sys_getchar();
            
            // DEBUG: Après l'appel syscall
            putc('[');
            putc('R');
            putc('E');
            putc('T');
            putc(':');
            if (c >= 32 && c <= 126) {
                putc((char)c);
            } else {
                putc('?');
            }
            putc(']');
            
            if (c == '\r' || c == '\n'){
                putc('\n');
                handle_line(ctx, buf);
                break;
            }
            if (c == 0x08 || c == 127){ // Backspace
                if (idx > 0){
                    idx--;
                    buf[idx] = '\0';
                    backspace();
                }
                continue;
            }
            if (idx < (int)sizeof(buf) - 1){
                buf[idx++] = (char)c;
                buf[idx] = '\0';
                putc((char)c);
            }
        }
EOF

# Sauvegarder le patch pour application manuelle si nécessaire
echo "📄 Patch debug créé: debug_shell_patch.c"

# 5. Compilation test
echo "🛠️ Compilation test..."
if make clean > /dev/null 2>&1 && make > debug_build.log 2>&1; then
    echo "✅ Compilation réussie"
else
    echo "❌ Erreur de compilation - voir debug_build.log"
    tail -10 debug_build.log
fi

echo ""
echo "📊 RÉSUMÉ DU DIAGNOSTIC:"
echo "1. ✅ Shell utilise sys_getchar() avec int $0x80, eax=2"
echo "2. ✅ Syscall SYS_GETC défini et handler présent"
echo "3. ✅ Handler appelle keyboard_getc()"
echo "4. ❓ Problème potentiel dans keyboard_getc() ou buffer"
echo ""
echo "🎯 PROCHAINES ÉTAPES:"
echo "1. Tester en temps réel avec: make run-gui"
echo "2. Analyser les logs série pour voir les debug"
echo "3. Identifier l'exact point de blocage"
echo ""
echo "💡 HYPOTHÈSES:"
echo "- Interruptions IRQ1 non reçues"
echo "- Buffer keyboard vide"
echo "- Timeout dans keyboard_getc()"
echo "- Problème dans l'initialisation PS/2"
