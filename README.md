# AI-OS - noyau pédagogique i386

[![Version](https://img.shields.io/badge/version-7-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Status](https://img.shields.io/badge/status-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![CI](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml/badge.svg)](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

AI-OS est un **prototype de hobby OS i386 32-bit** (Multiboot). Il boote sous QEMU, isole Ring 0/3 et lance un shell ELF. Le noyau charge un initrd TAR, fournit un overlay RAM persisté sur un disque IDE QEMU et expose une ABI de syscalls (`include/os_syscalls.h`).

Un moteur **GPT-2 124M freestanding** est optionnel : si les poids sont présents dans `models/` au moment du `make all` / `make iso`, ils sont empaquetés dans l'initrd et `ai <texte>` appelle `SYS_GPT2_GENERATE`. Sans ces fichiers, le shell et les tests unitaires restent utilisables.

**État réel du code :** [docs/ETAT_REEL.md](docs/ETAT_REEL.md) - source de vérité fonctionnelle. Index : [docs/README.md](docs/README.md).

La branche par défaut est **`master`**.

## Fonctionnalités

- **Shell Ring 3** - prompt `/ (-.-) :`. `ls`/`cat` lisent initrd + overlay ; `mkdir`/`rm`/`cp`/`mv`/`touch`/`write`/`append` mutent l'overlay (snapshot ATA PIO si un disque IDE est présent). `ps`/`kill`/`getpid`/`mem`/`uptime` interrogent le noyau.
- **GPT-2 local optionnel** - `ai <texte>` -> `[GPT-2 local]` si le checkpoint est dans l'initrd ; sinon message d'indisponibilité. Le binaire historique `bin/ai_assistant` reste empaqueté.
- **Isolation** - GDT/IDT, Ring 0/3, chargeur ELF, syscalls (0-22, `MAX_SYSCALLS` = 23).
- **Tâches** - passage kernel -> shell via `jump_to_task()` ; `spawn`/`yield`/`exec` basculent de façon coopérative (int 0x80). Le round-robin à chaque tick PIT n'est pas le mode actuel.
- **Fichiers** - initrd TAR en lecture seule + overlay RAM 32 nœuds (fichiers <= 256 octets). Snapshot persisté sur le disque IDE QEMU (`build/overlay.img`) en ATA PIO LBA28, sans IRQ14.
- **Matériel QEMU** - PIC, clavier PS/2, PIT 100 Hz, IDE primaire (maître). Historique clavier (EOI IRQ0) : [docs/ETAT_REEL.md](docs/ETAT_REEL.md).

Ce n'est **pas** un OS du quotidien : pas de réseau, pas de TLS/OpenAI effectif, pas d'interface graphique native.

## Démarrage rapide

Debian / Ubuntu (mêmes paquets que la CI) :

```bash
git clone https://github.com/kamgueblondin/ai-os.git
cd ai-os
make deps          # ou : bash scripts/bootstrap-dev.sh
make all
make test-all      # 144 tests Unity, sans poids GPT-2
make run           # QEMU curses ; le shell lit le clavier PS/2, pas le port série
```

`make deps` installe `build-essential`, `gcc-multilib`, `libc6-dev-i386`, `nasm`, `qemu-system-x86` et `python3`. Vérification locale : `make check-build-deps`.

| Cible | Rôle |
|---|---|
| `make all` | Noyau + initrd + `build/overlay.img` (disque IDE 32 Kio) |
| `make test-all` | Unity 32-bit (`test_pmm` 17, `test_syscall` 48, `test_task` 21, `test_overlay` 6, `test_tokenizer` 13, `test_gpt2_sample` 4, `test_shell` 25, `test_ramfs` 10) |
| `make qemu-smoke` | Cinq boots QEMU (overlay + extras + persist + spawn/yield + exec), sans modèle |
| `make ci` | `all` + `test-all` + `qemu-smoke` (gate PR) |
| `make run` / `make run-gui` | Session interactive (voir [docs/GUIDE_EXECUTION.md](docs/GUIDE_EXECUTION.md)) |

ISO bootable (GRUB, optionnel) :

```bash
sudo apt-get install -y grub-pc-bin xorriso
make iso && make run-iso
```

## GPT-2 local (optionnel)

Les poids **ne sont pas** dans Git. Assets validés : [release `gpt2-124m-assets`](https://github.com/kamgueblondin/ai-os/releases/tag/gpt2-124m-assets).

```text
models/
├── gpt2_124M.bin
└── gpt2_tokenizer.bin
```

```bash
mkdir -p models
curl -L -o models/gpt2_124M.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_124M.bin
curl -L -o models/gpt2_tokenizer.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_tokenizer.bin
curl -L -o models/gpt2-124m-assets.sha256 https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2-124m-assets.sha256
(cd models && sha256sum -c gpt2-124m-assets.sha256)
make all
```

| Fichier | SHA-256 |
|---|---|
| `gpt2_124M.bin` | `3da8b207584030bcdcd207cf7a99952e3421dce92da218b351071857511bf162` |
| `gpt2_tokenizer.bin` | `6f3abc21e444e4e8300e225f4e03da48ea121cf17e30f67009b8dad7a66c2f13` |

QEMU a besoin d'**1 Gio** de RAM pour ce checkpoint (`GPT2_RAM ?= 1024M`). Sans modèle, ~128 Mio suffisent pour le hobby shell.

Dans le shell : `ai hello`, `ai-provider local`, `ai-model list`, `ai-runtime`, `rc`. Détail déploiement : [docs/gpt2_baremetal_deployment.md](docs/gpt2_baremetal_deployment.md).

Le cache KV + SSE2 a ramené `ai hello` + 4 jetons de ~89 s à **~7,7 s** sous QEMU Pentium III sans KVM. L'objectif &lt; 1 s n'est pas atteint ici. Voir [docs/kv_cache_performance_report.md](docs/kv_cache_performance_report.md).

`make gpt2-recovery` / `gpt2-benchmark` / `gpt2-tests` exigent les fichiers sous `models/`. La CI GitHub **ne télécharge pas** le modèle.

## Tests

```bash
make test-all      # référence, sans poids
make qemu-smoke    # smoke QEMU (CI)
make ci            # même gate que GitHub Actions
```

Unity 32-bit : **144** tests (`test_pmm` 17, `test_syscall` 48, `test_task` 21, `test_overlay` 6, `test_tokenizer` 13, `test_gpt2_sample` 4, `test_shell` 25, `test_ramfs` 10). Les dossiers `tests/integration`, `tests/system`, `tests/performance` et `tests/robustness` sont vides. Les pourcentages de "couverture" affiches par d'anciens scripts ne sont pas mesures par gcov.

GitHub Actions (`.github/workflows/ci.yml`) : à chaque push/PR vers `master` (et `main` si la branche est renommée) - `make all`, `make test-all`, `make qemu-smoke`.

Guide : [docs/guide_tests_regression.md](docs/guide_tests_regression.md).

## Architecture

```
ai-os/
├── boot/                 # Multiboot, ISR stubs
├── kernel/               # GDT, IDT, PIC, clavier, timer, tâches, syscalls
│   ├── mem/              # PMM / VMM / heap
│   ├── task/
│   ├── syscall/
│   ├── input/            # buffer clavier
│   └── llm/              # GPT-2 freestanding
├── fs/                   # initrd TAR + overlay RAM (snapshot IDE)
├── userspace/            # shell, idle, ok, fake_ai, ai_assistant
├── include/              # ABI syscalls
├── initrd_content/       # contenu empaqueté dans my_initrd.tar
├── models/               # local, gitignoré - poids GPT-2 optionnels
├── tests/                # Unity + scripts QEMU
├── scripts/              # bootstrap-dev.sh
├── docs/
├── .cursor/environment.json
└── .github/workflows/ci.yml
```

## Roadmap (extrait)

Le dossier [`US/`](US/README.md) décrit la vision MOHHOS (spécifications, pas l'état du dépôt).

- [x] Inférence GPT-2 locale + cache KV / SSE2
- [x] Tokenizer BPE GPT-2 (entree et sortie)
- [x] Overlay persisté (snapshot ATA PIO sur disque IDE QEMU)
- [x] `spawn` / `yield` coopératifs (l'enfant tourne vraiment)
- [x] `exec` bloquant coopératif (parent TASK_WAITING, enfant SYS_EXIT)
- [ ] Quantification, chargeur GGUF, latence &lt; 1 s
- [ ] Réseau, DNS, TLS, fournisseur OpenAI effectif
- [ ] Système de fichiers disque général (ext2/FAT)

## Documentation

- [docs/ETAT_REEL.md](docs/ETAT_REEL.md) - ce qui tourne vraiment
- [docs/README.md](docs/README.md) - index (actuel vs historique)
- [docs/GUIDE_EXECUTION.md](docs/GUIDE_EXECUTION.md) - modes QEMU
- [docs/gpt2_baremetal_deployment.md](docs/gpt2_baremetal_deployment.md) - ISO autonome avec modèle

Les rapports "clavier FIXED" v6.0/v6.1 sont **historiques**. Le correctif actuel (EOI IRQ0 avant `schedule()`) est dans ETAT_REEL.

## Contribution

```bash
make deps && make ci
```

Code commenté en français. Une PR = une tranche visible par la CI (`make ci`), sans artefacts `build/`, ELFs userspace ni `models/`.

**Dépôt :** [github.com/kamgueblondin/ai-os](https://github.com/kamgueblondin/ai-os)

*Prototype i386 + shell userspace + GPT-2 local optionnel sous QEMU. Dernière mise à jour documentation : 2026-08-13, overlay persisté sur IDE.*
