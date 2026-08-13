# User Stories — AI-OS et vision MOHHOS

Deux couches distinctes. Ne pas les mélanger.

| Couche | Document | Statut |
|---|---|---|
| **Prototype qui tourne** | [ai_os_us.md](ai_os_us.md) + [docs/ETAT_REEL.md](../docs/ETAT_REEL.md) | AOS-001 à AOS-025 et IPC Foundation MOHHOS vérifiés |
| **Vision MOHHOS** | fichiers `mohhos_*.md` + [individual_us/](individual_us/INDEX.md) | Spécifications, sauf incrément Foundation IPC documenté |

En cas de contradiction, **ETAT_REEL** et **ai_os_us.md** priment.

## Couche 1 — AI-OS (à utiliser)

Hobby OS i386 32-bit : boot QEMU, shell Ring 3, overlay persisté, `spawn`/`yield`/`exec` coopératifs, GPT-2 124M optionnel.

- Backlog réel : [ai_os_us.md](ai_os_us.md) (`AOS-001` … `AOS-012` faits, `AOS-020` … `AOS-025` suite)
- Runtime : [docs/ETAT_REEL.md](../docs/ETAT_REEL.md)
- Roadmap courte : [README.md](../README.md)

Ce n'est **pas** TensorFlow Lite, pas un microkernel, pas `fake_ai` comme moteur principal (`fake_ai` est un binaire historique ; `ai <texte>` appelle `SYS_GPT2_GENERATE`).

## Couche 2 — MOHHOS (archives de conception)

Plan historique pour transformer AI-OS v5 en « Manus Operating Hybrid Hosted OS » (8 phases, 120 US, ~1640 j-h). Trois incréments de **Foundation** sont maintenant livrés : une boîte aux lettres IPC locale, un médiateur VFS Ring 3 de lecture et un registre nommé qui permet au client de résoudre `vfs` sans PID codé en dur. Ils préparent US-001/US-003/US-012/US-013, mais ne déplacent encore ni backend VFS, ni pilotes, ni réseau vers un espace d’adressage séparé ; le noyau reste monolithique.

Les autres fichiers MOHHOS restent des **spécifications**. Le recouvrement avec le prototype (mémoire, tests, moteur IA local, assistant, IPC local, médiateur VFS et découverte de service) est partiel : voir le tableau dans [individual_us/INDEX.md](individual_us/INDEX.md). Un ✅ dans l’index MOHHOS signifie « fichier de spec présent », **pas** « implémenté », sauf lorsqu’un statut explicite de tranche livrée est indiqué.

### Fichiers MOHHOS

| Fichier | Contenu |
|---|---|
| [mohhos_user_stories_master.md](mohhos_user_stories_master.md) | Index historique des 120 titres (numérotation parfois **différente** des fichiers) |
| [recherche_technologies_mohhos.md](recherche_technologies_mohhos.md) | Veille (P2P, federated learning, navigateur-OS) |
| [mohhos_us_phase1_foundation.md](mohhos_us_phase1_foundation.md) | Phase 1 détaillée : IPC, médiateur VFS et découverte nommée livrés ; microkernel/services séparés non commencés |
| [../docs/mohhos_foundation_increment_01_ipc.md](../docs/mohhos_foundation_increment_01_ipc.md) | Conception et contrat de l’incrément IPC Foundation livré |
| [../docs/mohhos_foundation_increment_02_vfs_service.md](../docs/mohhos_foundation_increment_02_vfs_service.md) | Médiateur VFS Ring 3 et contrat de lecture IPC livré |
| [../docs/mohhos_foundation_increment_03_service_registry.md](../docs/mohhos_foundation_increment_03_service_registry.md) | Registre nommé, découverte `vfs` et limites de sécurité |
| [mohhos_us_phase2_ai_core.md](mohhos_us_phase2_ai_core.md) | Phase 2 (TensorFlow Lite, NLU, fédéré) — non livrée ; l'IA réelle est GPT-2 freestanding |
| [mohhos_us_phase3_web_runtime.md](mohhos_us_phase3_web_runtime.md) | Phase 3 navigateur-OS — absente |
| [mohhos_us_phases_4_8_synthese.md](mohhos_us_phases_4_8_synthese.md) | Phases 4-8 (PromptMessage, P2P, etc.) — absentes |
| [individual_us/](individual_us/INDEX.md) | ~78 fichiers de spec ; IDs **023/024/025 dupliqués** ; pas 120 fichiers |

### Phases MOHHOS (rappel)

1. Foundation — microkernel, plugins, logging distribué  
2. AI Core — TFLite, NLU, apprentissage fédéré, cloud-edge  
3. Web Runtime — navigateur comme FS  
4. PromptMessage — langage universel  
5. P2P Network  
6. Multi-platform  
7. Collaborative (points)  
8. Production  

La migration complète de US-001 reste une refonte à haut risque : les incréments actuels fournissent un mécanisme IPC, un médiateur VFS de lecture et une découverte de nom. La suite doit introduire des droits de publication/découverte, corréler les réponses, externaliser le backend VFS lui-même, puis déplacer pilotes ou réseau derrière ces droits, sans affirmer prématurément que ces composants sont déjà hors du noyau.

## Contribution

1. Une PR = une tranche visible par `make ci` (code) **ou** un alignement doc (ce dossier).  
2. Nouvelles user stories du prototype : les ajouter dans `ai_os_us.md` (préfixe `AOS-`), pas en renumérotant MOHHOS.  
3. Les specs MOHHOS peuvent rester ; mettre à jour seulement la bannière « état réel » si le recouvrement change.
