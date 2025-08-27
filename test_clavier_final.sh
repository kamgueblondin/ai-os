#!/bin/bash

echo "=== TEST INTERACTIF DU CLAVIER AI-OS ==="
echo "Lancement d'AI-OS avec test automatique..."

cd /workspace/ai-os

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
