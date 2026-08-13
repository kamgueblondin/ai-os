# Documentation AI-OS

## Lire en premier

1. [ETAT_REEL.md](ETAT_REEL.md) — **état actuel du code**, y compris GPT-2 local et limites vérifiées
2. [gpt2_baremetal_deployment.md](gpt2_baremetal_deployment.md) — préparation des poids et construction d’une ISO autonome
3. [kv_cache_performance_report.md](kv_cache_performance_report.md) — cache KV, reprise du shell et mesures de latence
4. [GUIDE_EXECUTION.md](GUIDE_EXECUTION.md) — lancement QEMU (console / GUI / nographic)
5. [../README.md](../README.md) — compilation, tests, architecture des sources

Les autres fichiers de ce dossier sont conservés : rapports de debug, chronologie des correctifs clavier, spécifications d’étapes. Beaucoup décrivent un état **intermédiaire** (shell simulé dans le kernel, crash timer, clavier mort). Ils ne sont pas effacés.

## Documents à jour (comportement courant)

| Fichier | Contenu |
|---|---|
| [ETAT_REEL.md](ETAT_REEL.md) | État fonctionnel, GPT-2 local et limites vérifiées |
| [gpt2_baremetal_deployment.md](gpt2_baremetal_deployment.md) | Préparation des artefacts, construction et démarrage d’une ISO GPT-2 hors ligne |
| [kv_cache_performance_report.md](kv_cache_performance_report.md) | Cache KV, SSE2, test de reprise du shell et mesures de latence |
| [baremetal_llm_architecture.md](baremetal_llm_architecture.md) | Architecture de référence et évolutions envisagées pour un LLM bare-metal |
| [GUIDE_EXECUTION.md](GUIDE_EXECUTION.md) | Modes QEMU et dépannage clavier |
| [CHANGELOG_v6.1.md](CHANGELOG_v6.1.md) | Notes de version 6.1 + correctif EOI |
| [todo.md](todo.md) | Suivi des tâches (historique + reste à faire) |
| [etape_7_shell_ia.md](etape_7_shell_ia.md) | Étape shell et ancien simulateur IA ; lecture historique |
| [analyse_logique_docs.md](analyse_logique_docs.md) | Logique des étapes 1–7 |
| [etapes_3_4_specifications.md](etapes_3_4_specifications.md) | Specs mémoire / initrd |
| [etapes_5_6_implementation.md](etapes_5_6_implementation.md) | Multitâche / userspace |
| [AI_OS_v5_NOUVELLES_FONCTIONNALITES.md](AI_OS_v5_NOUVELLES_FONCTIONNALITES.md) | Fonctionnalités v5 |

## Diagnostics et correctifs (historiques, contenu technique conservé)

Utile pour comprendre *pourquoi* un correctif a été tenté. La conclusion « toujours cassé » peut être fausse aujourd’hui.

### Architecture et userspace

- [ANALYSE_ARCHITECTURE.md](ANALYSE_ARCHITECTURE.md)
- [ANALYSE_PROBLEMES.md](ANALYSE_PROBLEMES.md)
- [ANALYSE_PROBLEMES_SHELL.md](ANALYSE_PROBLEMES_SHELL.md)
- [diagnostic_problemes.md](diagnostic_problemes.md)
- [DIAGNOSTIC_SHELL_IA_PROBLEMES.md](DIAGNOSTIC_SHELL_IA_PROBLEMES.md)
- [DIAGNOSTIC_COMPLET.md](DIAGNOSTIC_COMPLET.md)
- [CORRECTION_SHELL_V5.md](CORRECTION_SHELL_V5.md)
- [CORRECTION_STABILITE.md](CORRECTION_STABILITE.md)

### Clavier PS/2 (plusieurs itérations)

- [CORRECTION_CLAVIER_FINALE.md](CORRECTION_CLAVIER_FINALE.md)
- [CORRECTION_CLAVIER_FINALE_V2.md](CORRECTION_CLAVIER_FINALE_V2.md)
- [CORRECTION_CLAVIER_ULTIME.md](CORRECTION_CLAVIER_ULTIME.md)
- [CORRECTION_CLAVIER_DEFINITIVE_v7.md](CORRECTION_CLAVIER_DEFINITIVE_v7.md)
- [CORRECTION_DEFINITIVE_CLAVIER_FINALE.md](CORRECTION_DEFINITIVE_CLAVIER_FINALE.md)
- [CORRECTION_AFFICHAGE_CLAVIER_V6.md](CORRECTION_AFFICHAGE_CLAVIER_V6.md)
- [CORRECTIONS_CLAVIER.md](CORRECTIONS_CLAVIER.md)
- [KEYBOARD_STABLE_FIX.md](KEYBOARD_STABLE_FIX.md)
- [KEYBOARD_ULTIMATE_FIX.md](KEYBOARD_ULTIMATE_FIX.md)
- [DIAGNOSTIC_CLAVIER_V3.md](DIAGNOSTIC_CLAVIER_V3.md)
- [DIAGNOSTIC_FINAL_CLAVIER_RESOLU.md](DIAGNOSTIC_FINAL_CLAVIER_RESOLU.md)
- [RAPPORT_CORRECTION_CLAVIER_FINALE.md](RAPPORT_CORRECTION_CLAVIER_FINALE.md)

Le blocage restant (EOI IRQ0 après `schedule()` / PIC qui masque IRQ1) est documenté dans [ETAT_REEL.md](ETAT_REEL.md), pas dans ces itérations.

### Rapports de campagne de tests / implémentation

- [RAPPORT_IMPLEMENTATION.md](RAPPORT_IMPLEMENTATION.md)
- [RAPPORT_CORRECTION_FINALE.md](RAPPORT_CORRECTION_FINALE.md)
- [RAPPORT_DIAGNOSTIC_APPROFONDI.md](RAPPORT_DIAGNOSTIC_APPROFONDI.md)
- [RAPPORT_TEST_FINAL_v7.md](RAPPORT_TEST_FINAL_v7.md)
- [RAPPORT_FINAL_V2.md](RAPPORT_FINAL_V2.md)
- [RAPPORT_FINAL_V4.md](RAPPORT_FINAL_V4.md)
- [rapport_tests_shell_ia.md](rapport_tests_shell_ia.md)
- [rapport_stabilite_modules.md](rapport_stabilite_modules.md)

Les fichiers `.log` / `.txt` du même dossier sont des captures QEMU ou de build : traces brutes, pas de spécification. Les scripts `tests/scripts/test_gpt2_shell_recovery.py` et `tests/scripts/benchmark_gpt2_kv_latency.py` sont les contrôles QEMU correspondants ; ils requièrent les poids locaux sous `models/`.

## Vision MOHHOS (hors code actuel)

Dossier [../US/](../US/README.md) : 120 user stories et phases Foundation → Production. Ce sont des **spécifications cibles**, pas l’état du dépôt. Légende dans [../US/individual_us/INDEX.md](../US/individual_us/INDEX.md).
