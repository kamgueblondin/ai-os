# Guide d'Exécution AI-OS - Modes Console et GUI

## 🚀 Options de Lancement

### 1. Mode Console Optimal (Recommandé)
```bash
make run
```
- **Affichage** : Mode texte dans le terminal avec curses
- **Clavier** : Pleinement fonctionnel avec interruptions PS/2
- **Compatible** : Linux, macOS, Windows avec QEMU
- **Avantages** : Pas de fenêtre séparée, performance optimale

### 2. Mode Interface Graphique
```bash
make run-gui
```
- **Affichage** : Fenêtre QEMU graphique
- **Clavier** : Pleinement fonctionnel
- **Compatible** : Environnements avec interface graphique
- **Avantages** : Interface familière, debugging visuel

### 3. Mode Nographic (Fallback)
```bash
make run-nographic
```
- **Affichage** : Redirection complète vers terminal
- **Clavier** : Peut être limité selon l'environnement
- **Usage** : Uniquement si les autres modes échouent

## 🔧 Dépannage Clavier

### Problème : Clavier non-responsif en mode console
**Solution** : Utilisez `make run` (curses) au lieu de `make run-nographic`

### Problème : Caractères incorrects affichés
**Vérification** : La table PS/2 Set 1 est maintenant corrigée

### Problème : Touches qui ne répondent pas
**Debug** : Vérifiez les logs série pour les messages d'initialisation du clavier

## 📋 Configuration Technique

### Mode Console (`make run`)
- **Display** : `-display curses`
- **Série** : Configuration séparée avec `-chardev stdio`
- **Interruptions** : PS/2 IRQ1 préservées
- **Scancode** : PS/2 Set 1 avec translation activée

### Mode GUI (`make run-gui`)
- **Display** : `-display gtk`
- **VGA** : Support graphique standard
- **Série** : Configuration distincte
- **Interruptions** : Optimales

## 🔍 Messages de Debug

Surveillez ces messages au démarrage :
```
=== KEYBOARD HYBRID INIT (FIXED) ===
Phase 1: Nettoyage initial...
Phase 2: Configuration PS/2 renforcée...
Configuration actuelle: 0xXX
Nouvelle configuration: 0xXX
Phase 3: Configuration périphérique...
Phase 4: Finalisation...
=== KEYBOARD INIT COMPLETE ===
```

## ⚡ Performance

- **Mode Console** : Latence minimale, idéal pour développement
- **Mode GUI** : Meilleur pour démonstrations et tests visuels
- **Hybride** : Interruptions + polling de secours automatique

## 🐛 Résolution de Problèmes

1. **Clavier bloqué** : Redémarrer QEMU (Ctrl+A puis X en mode console)
2. **Pas de réponse** : Vérifier que le mode QEMU est compatible
3. **Caractères étranges** : Vérifier la configuration du terminal hôte

---
*Guide mis à jour pour AI-OS v6.0 avec corrections clavier complètes*
