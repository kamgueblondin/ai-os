# État réel d’AI-OS

**Date de constat :** 13 août 2026  
**Code de référence :** branche `cursor/shell-missing-builtins-6f7a` (builtins shell + VFS RAM)  
**Rôle de ce document :** source de vérité sur ce qui **tourne réellement**, par rapport aux diagnostics historiques et à la vision MOHHOS.

Les rapports, TODO et user stories plus anciens restent utiles (pistes de debug, extraits de code, spécifications). Ils ne décrivent plus forcément le comportement actuel. En cas de contradiction, **ce fichier prime**.

## Synthèse

AI-OS est un **prototype de noyau pédagogique i386 32-bit**. Il boote sous QEMU, charge un initrd TAR, passe en espace utilisateur (Ring 3) et exécute un shell ELF interactif. L’« IA » est un **simulateur par mots-clés** (`userspace/fake_ai.c`), pas un moteur d’inférence.

| Périmètre | Avancement réel |
|---|---|
| Hobby OS minimal (boot → shell sous QEMU) | ~60–70 % |
| Produit « OS pour l’IA » décrit dans d’anciens README | ~15–25 % |
| Vision MOHHOS (120 US, 8 phases) | ~1–2 % (spécifications + simulateur) |

Ce n’est **pas** un système d’exploitation utilisable au quotidien (pas de FS persistant, pas de réseau, pas d’interface graphique native, pas de vrais pilotes hors QEMU/i8042).

## Ce qui fonctionne (vérifié)

Constaté par compilation `make all`, boot QEMU (nographic et GTK), saisie clavier via `sendkey`, et `make test-all`.

### Noyau

- Boot Multiboot, affichage VGA + logs série
- GDT, IDT, PIC 8259, PIT (timer), clavier PS/2 (IRQ1)
- PMM / VMM / heap, paging
- Initrd format TAR POSIX (`fs/initrd.c`)
- Chargeur ELF 32-bit
- Tâches kernel + utilisateur, `jump_to_task()` (iret vers Ring 3)
- Syscalls : `SYS_EXIT`, `SYS_PUTC`, `SYS_GETC`, `SYS_PUTS`, `SYS_YIELD`, `SYS_GETS`, `SYS_EXEC`, `SYS_SPAWN`

### Espace utilisateur

- `userspace/shell.c` s’exécute vraiment en Ring 3 (plus de boucle shell simulée dans le kernel)
- Programmes empaquetés dans l’initrd : `shell`, `fake_ai`, `ai_assistant`, `user_program`
- Commande `ai <texte>` lance `fake_ai` via `SYS_EXEC`

### Clavier (août 2026)

Les nombreuses notes « clavier corrigé » des versions 6.0/6.1 étaient **partiellement** vraies (init PS/2, scancodes, buffer). Un bug restait : `schedule()` fait `iret` et ne revient jamais dans le stub IRQ0, donc l’EOI PIC placé *après* `schedule()` n’était pas envoyé. IRQ0 restait in-service et **bloquait IRQ1**.

Correctif actuel :

1. EOI IRQ0 **avant** `timer_handler` / `schedule()` (`boot/isr_stubs.s`)
2. Planification seulement si `g_reschedule_needed` (lancement du shell, exec/spawn) — plus à chaque tick, ce qui provoquait un page fault une fois l’EOI rétabli (`kernel/timer.c`)

Après ce correctif : IRQ1 livrée, `help` / `ls` / `sysinfo` / `ai bonjour` reçus par `SYS_GETS`.

### Tests

`make test-all` (binaires 32-bit) :

| Binaire | Tests unitaires |
|---|---|
| `test_pmm` | 17/17 |
| `test_syscall` | 30/30 |
| `test_task` | 21/21 |
| `test_shell` | 25/25 |
| `test_ramfs` | 10/10 |

Pas de fichiers de tests dans `tests/integration`, `tests/system`, `tests/performance`, `tests/robustness`. Le compteur « Total Tests » du script `run_all_tests.sh` reste à 0 (incrément placé après `return`) : bug d’affichage, pas d’échec des binaires.

Dépendance de compilation 32-bit : paquet `gcc-multilib` / `libc6-dev-i386` (en plus de `nasm` et `qemu-system-i386`).

## Shell : commandes réelles vs affichées

Les commandes listées par `help` sont branchées dans `execute_builtin_command()`. Ce n’est **pas** un vrai disque ni le scheduler du noyau : fichiers et processus vivent dans le processus shell (`userspace/ramfs.c`, `userspace/procsim.c`).

| Commande | Comportement réel |
|---|---|
| `help` | Aide |
| `ls` / `dir` | Listing du **VFS RAM** (répertoire courant ou argument) |
| `mkdir` / `rmdir` / `rm` / `cp` / `mv` | Mutation du VFS RAM |
| `cat` / `grep` / `wc` / `sort` / `head` / `tail` | Lecture du VFS RAM |
| `echo` | Affichage ; `echo texte > fichier` écrit dans le VFS RAM |
| `cd` / `pwd` | Chemin en RAM, `cd` refuse un dossier absent du VFS |
| `ps` / `jobs` / `top` / `kill` | Table de processus **simulée** (`kill` ne touche pas le noyau ; pid 0/1 protégés) |
| `sysinfo` / `info` / `mem` / `memory` | Texte fixe (128 MB, etc.) |
| `uptime` / `date` | Horloge pédagogique (compteur de commandes, pas de RTC) |
| `whoami` / `env` / `export` | Variables d’environnement du shell (`USER=root` par défaut) |
| `alias` / `unalias` | Table d’alias, expansion avant exécution |
| `history` | Historique en mémoire |
| `which` | `builtin` ou `bin/<cmd> (non verifie)` |
| `clear`/`cls` | Séquence ANSI + bannière |
| `ai`, `ai-mode`, `ai-help`, `ai-test`, `ai-stats` | Pont vers le simulateur + compteur de requêtes |
| `exit` / `quit` / `logout` | Sortie du programme |
| `reboot` / `shutdown` | Message simulé (QEMU n’est pas arrêté) |

## « Intelligence artificielle »

`userspace/fake_ai.c` compare la question (minuscules) à des mots-clés et imprime une réponse préprogrammée (`bonjour` → `AI: bonjour`, `healthcheck` → `AI HEALTH: OK`, …). Il n’y a pas de TensorFlow Lite, pas de NLP, pas de modèle, pas d’apprentissage.

`initrd_content/ai_knowledge.txt` et `ai_data.txt` sont des fichiers texte d’accompagnement, pas une base vectorielle.

## Ce qui n’existe pas dans le code

Malgré la roadmap README v7/v8 et le dossier `US/` :

- Moteur d’IA réel, apprentissage fédéré, cloud-edge
- Système de fichiers persistant (disque)
- Pile TCP/IP, services réseau
- Interface graphique du OS (seul QEMU affiche du VGA texte)
- Architecture microkernel, IPC, plugins
- Réseau P2P, multi-plateforme, économie collaborative
- Les 120 User Stories MOHHOS : **spécifications**, pas d’implémentation (sauf recouvrement accidentel avec le noyau actuel : mémoire, tests unitaires partiels)

Le préemptif « à chaque tick » est volontairement **limité** : après le premier passage au shell, le timer n’appelle plus `schedule()` sauf `g_reschedule_needed`. Le round-robin continu n’est pas le mode de production actuel (stabilité).

## Documents historiques à ne pas prendre pour l’état courant

Ces fichiers restent utiles (chronologie, extraits, hypothèses). Leur conclusion « le shell ne se lance pas » / « crash timer » / « clavier mort à jamais » est **obsolète** :

- `docs/todo.md` (phases 3–4 : blocage userspace) — mis à jour, le diagnostic d’origine est conservé
- `docs/ANALYSE_ARCHITECTURE.md`, `ANALYSE_PROBLEMES.md`, `ANALYSE_PROBLEMES_SHELL.md`
- `docs/diagnostic_problemes.md`, `DIAGNOSTIC_SHELL_IA_PROBLEMES.md`
- Série `CORRECTION_CLAVIER_*.md`, `DIAGNOSTIC_CLAVIER_*.md`, `KEYBOARD_*.md` (correctifs PS/2 toujours pertinents ; le blocage PIC/EOI n’y est en général pas identifié)

## Comment vérifier

```bash
sudo apt-get install -y build-essential gcc-multilib nasm qemu-system-i386
make clean && make all
make test-all
make qemu-smoke   # boot QEMU headless, exige le prompt dans le log série
make ci           # all + test-all + qemu-smoke (même gate que GitHub Actions)
make run          # console curses (recommandé en local)
make run-gui      # fenêtre GTK
```

GitHub Actions (`.github/workflows/ci.yml`) lance ce gate sur chaque push et pull request vers `master`. Les anciens workflows `tests.yml` / `cmake-test.yml` visaient `main`/`develop` et ne s’exécutaient donc pas.

En nographic, le shell lit le **clavier PS/2**, pas le port série : la saisie TTY hôte n’atteint souvent pas `SYS_GETS`. Préférer curses/GTK, ou QEMU `sendkey` / moniteur.

Prompt actuel du shell : `/ (-.-) :`
