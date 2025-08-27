#!/bin/bash

# Script d'application de la correction clavier AI-OS
# Applique automatiquement tous les correctifs et commit sur GitHub

set -e

echo "=== APPLICATION CORRECTION CLAVIER AI-OS ==="
echo "Correction du problème des codes PS/2 ACK au lieu des scancodes"
echo ""

# Vérifier que nous sommes dans le bon répertoire
if [ ! -f "Makefile" ] || [ ! -d "kernel" ]; then
    echo "ERREUR: Veuillez exécuter ce script depuis le répertoire racine d'AI-OS"
    exit 1
fi

# Étape 1: Appliquer la correction du driver clavier
echo "🔧 Étape 1: Application de la correction du driver clavier..."
if [ -f "kernel/keyboard_fixed.c" ]; then
    echo "   - Sauvegarde de l'ancien driver: kernel/keyboard.c.backup"
    cp kernel/keyboard.c kernel/keyboard.c.backup
    
    echo "   - Application de la nouvelle version corrigée"
    cp kernel/keyboard_fixed.c kernel/keyboard.c
    
    echo "   ✅ Driver clavier corrigé appliqué"
else
    echo "   ❌ ERREUR: Fichier kernel/keyboard_fixed.c non trouvé!"
    exit 1
fi

# Étape 2: Rendre les scripts exécutables
echo ""
echo "🛠️ Étape 2: Configuration des permissions des scripts..."
chmod +x test_keyboard_ultimate.sh
chmod +x apply_keyboard_fix.sh
echo "   ✅ Permissions configurées"

# Étape 3: Test de compilation pour vérifier que tout fonctionne
echo ""
echo "🔨 Étape 3: Test de compilation..."
make clean > /dev/null 2>&1
if make > compile_test.log 2>&1; then
    echo "   ✅ Compilation réussie"
    rm -f compile_test.log
else
    echo "   ❌ ERREUR de compilation!"
    echo "   Voir compile_test.log pour les détails"
    exit 1
fi

# Étape 4: Créer un rapport de correction
echo ""
echo "📝 Étape 4: Génération du rapport de correction..."
cat > CORRECTION_CLAVIER_ULTIME.md << 'EOF'
# Correction Ultime du Problème Clavier AI-OS

## Problème Identifié

Le clavier de l'AI-OS ne fonctionnait pas correctement car :
1. Le handler IRQ1 était appelé mais ne recevait que des codes PS/2 ACK (0xFA) au lieu de scancodes
2. L'initialisation du contrôleur PS/2 était incomplète
3. Les délais entre les opérations PS/2 étaient insuffisants
4. La gestion des codes de contrôle PS/2 n'était pas optimale

## Solution Implémentée

### Corrections du Driver (`kernel/keyboard.c`)

1. **Initialisation PS/2 Complète** :
   - Ajout d'un reset complet du périphérique clavier (commande 0xFF)
   - Délais appropriés entre les opérations PS/2
   - Vidage systématique des buffers à chaque étape
   - Configuration explicite du scancode set 1

2. **Gestion Améliorée des Codes PS/2** :
   - Distinction claire entre les codes de contrôle (0xFA, 0xFE, 0xAA) et les scancodes
   - Handler d'interruption optimisé avec moins de debug spam
   - Timeout amélioré dans keyboard_getc()

3. **Stabilisation** :
   - Ajout de délais de stabilisation après l'initialisation
   - Fonctions d'attente robustes pour les opérations PS/2
   - Gestion d'erreur améliorée

### Script de Test Optimisé (`test_keyboard_ultimate.sh`)

1. **Paramètres QEMU Spécialisés** :
   - CPU Pentium3 avec meilleur support PS/2
   - Interface graphique GTK native
   - Émulation clavier PS/2 complète
   - Logs d'interruptions détaillés

2. **Processus de Test Guidé** :
   - Instructions claires pour l'utilisateur
   - Application automatique des corrections
   - Vérification de compilation
   - Lancement QEMU optimisé

## Résultats Attendus

Après cette correction :
- ✅ Le clavier devrait générer des vrais scancodes au lieu de codes ACK
- ✅ Le shell devrait répondre aux frappes de touches
- ✅ Les commandes devraient fonctionner normalement
- ✅ Les interruptions IRQ1 devraient traiter les caractères correctement

## Test de la Correction

```bash
# Appliquer la correction
./apply_keyboard_fix.sh

# Tester le clavier avec QEMU optimisé
./test_keyboard_ultimate.sh
```

## Fichiers Modifiés

- `kernel/keyboard.c` - Driver clavier corrigé
- `test_keyboard_ultimate.sh` - Script de test optimisé
- `apply_keyboard_fix.sh` - Script d'application des corrections
- `CORRECTION_CLAVIER_ULTIME.md` - Ce rapport

---
*Correction appliquée le $(date)*
EOF

echo "   ✅ Rapport généré: CORRECTION_CLAVIER_ULTIME.md"

# Étape 5: Préparer le commit
echo ""
echo "📦 Étape 5: Préparation du commit Git..."
echo "   - Ajout des nouveaux fichiers au git"

# Vérifier si c'est un repository git
if [ ! -d ".git" ]; then
    echo "   - Initialisation du repository git"
    git init
    git config user.name "kamgueblondin" 2>/dev/null || true
    git config user.email "kamgueblondin@gmail.com" 2>/dev/null || true
fi

# Ajouter les fichiers
git add kernel/keyboard.c
git add kernel/keyboard_fixed.c
git add test_keyboard_ultimate.sh
git add apply_keyboard_fix.sh
git add CORRECTION_CLAVIER_ULTIME.md

# Si il y a un backup, l'ajouter aussi
if [ -f "kernel/keyboard.c.backup" ]; then
    git add kernel/keyboard.c.backup
fi

echo "   ✅ Fichiers ajoutés au git"

echo ""
echo "🎉 CORRECTION APPLIQUÉE AVEC SUCCÈS !"
echo ""
echo "📋 PROCHAINES ÉTAPES :"
echo "   1. Testez la correction avec: ./test_keyboard_ultimate.sh"
echo "   2. Si le test fonctionne, commitez avec:"
echo "      git commit -m \"Correction ultime du problème clavier - Reset PS/2 complet et QEMU optimisé\""
echo "   3. Puis poussez sur GitHub avec:"
echo "      git push origin master"
echo ""
echo "🔍 FICHIERS MODIFIÉS :"
echo "   - kernel/keyboard.c (driver corrigé)"
echo "   - test_keyboard_ultimate.sh (test optimisé)"
echo "   - apply_keyboard_fix.sh (ce script)"
echo "   - CORRECTION_CLAVIER_ULTIME.md (rapport)"
echo ""
echo "📖 Lisez CORRECTION_CLAVIER_ULTIME.md pour les détails complets."
