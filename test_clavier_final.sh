#!/bin/bash

echo "=== TEST INTERACTIF DU CLAVIER AI-OS ==="
echo "Lancement d'AI-OS avec test automatique..."

# Déterminer le dossier du script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Test avec commandes simulées
{
    sleep 2
    echo "help"
    sleep 2  
    echo "ls"
    sleep 2
    echo "test"
    sleep 2
} | timeout 20 make run &

# Attendre un peu puis afficher les résultats
wait
