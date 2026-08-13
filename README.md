# AI-OS — noyau pédagogique i386

[![Version](https://img.shields.io/badge/version-7-blue.svg)](https://github.com/kamgueblondin/ai-os)
[![Statut](https://img.shields.io/badge/statut-prototype-yellow.svg)](https://github.com/kamgueblondin/ai-os)
[![CI](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml/badge.svg)](https://github.com/kamgueblondin/ai-os/actions/workflows/ci.yml)
[![Licence](https://img.shields.io/badge/licence-MIT-blue.svg)](LICENSE)

AI-OS est un **prototype de hobby OS i386 32-bit** démarrant par Multiboot. Il démarre sous QEMU, sépare Ring 0 et Ring 3, charge un initrd TAR et lance un shell ELF. Le noyau fournit une ABI de syscalls, un overlay RAM persistant via ATA PIO et un chemin d’inférence GPT-2 local optionnel.

> La source de vérité des fonctions réellement livrées est [docs/ETAT_REEL.md](docs/ETAT_REEL.md). La branche par défaut est `master`.

## Capacités vérifiées

| Domaine | Fonction réellement disponible |
|---|---|
| Démarrage | Multiboot BIOS, VGA/série, GDT, IDT, PIC, PIT et clavier PS/2 |
| Utilisateur | Shell ELF Ring 3, syscalls 0–28, `spawn`, `yield`, `exec`, `ps`, `kill` |
| Préemption | Quantum IRQ0 de 20 ticks, uniquement entre tâches utilisateur prêtes |
| IPC Foundation | Boîte aux lettres FIFO entre tâches Ring 3, 4 entrées par tâche, charge de 96 octets et `request_id` opaque ; pas de capabilities |
| VFS Foundation | `vfsserver` Ring 3, lecture initrd/overlay médiée par IPC et réponse corrélée ; backend encore noyau |
| Découverte Foundation | Registre volatile de 8 services ; retrait, transfert par propriétaire et purge immédiate à la terminaison ; pas de capabilities |
| Fichiers | Initrd TAR en lecture seule et overlay ATA PIO V2 persistant (64 nœuds, V1 compatible) |
| IA locale | GPT-2 124M `llm.c v3`, BPE UTF-8, cache KV, SSE2 et top-k, sans réseau au boot |
| GGUF | Sonde structurelle GGUF v3 et primitives Q8_0 ; pas encore d’inférence quantifiée |
| Réseau | `net-status` et profil OpenAI explicitement bloqué ; aucune pile réseau noyau |

Les commandes du shell comprennent notamment `ls`, `cat`, `mkdir`, `rmdir`, `rm`, `cp`, `mv`, `write`, `append`, `touch`, `stat`, `grep`, `wc`, `spawn`, `yield`, `ipc-send`, `ipc-recv`, `service-publish`, `service-grant`, `service-find`, `vfs-read <fichier>`, `jobs`, `top`, `ai`, `ai-provider`, `ai-model`, `ai-runtime` et `net-status`. `vfs-read` résout le service `vfs` au lieu d’accepter un PID. Les programmes initrd incluent `shell`, `idle`, `spin`, `ipcserver`, `vfsserver`, `serviceclaim`, `ok`, `fake_ai`, `ai_assistant` et `user_program`.

## Démarrage rapide

Sur Debian ou Ubuntu, installez les dépendances puis construisez le noyau et l’initrd.

```bash
git clone https://github.com/kamgueblondin/ai-os.git
cd ai-os
make deps
make all
make test-all
make integration-qemu
make run
```

| Cible | Rôle |
|---|---|
| `make all` | Noyau, initrd et image overlay IDE de 64 secteurs |
| `make test-all` | 182 tests C Unity/robustesse sans dépendre des poids GPT-2 |
| `make qemu-smoke` | Scénarios QEMU classiques : overlay, persistance, spawn/yield et exec |
| `make integration-qemu` | Contrats QEMU AOS-022, AOS-024, AOS-025, IPC, VFS, cycle de vie et transfert Foundation |
| `make qemu-irq0-preemption` | Lance `spin` puis exige un shell toujours réactif |
| `make qemu-ai-provider` | Vérifie le diagnostic réseau et le blocage OpenAI |
| `make qemu-ipc-foundation` | Lance `ipcserver`, envoie un message et vérifie sa réception |
| `make qemu-vfs-service` | Lance `vfsserver`, lit `hello.txt` via IPC corrélé puis vérifie le cycle de vie |
| `make qemu-service-grant` | Publie `demo`, le transfère à un autre PID et vérifie son nettoyage |
| `make iso` | Produit l’ISO BIOS/GRUB bootable |
| `make run` / `make run-gui` | Session QEMU interactive curses ou GTK |

Pour construire l’ISO, installez également GRUB et xorriso.

```bash
sudo apt-get install -y grub-pc-bin xorriso
make iso
make run-iso
```

## GPT-2 local, sans réseau au démarrage

Les poids ne sont **pas** dans Git. Les actifs validés sont distribués par la [release `gpt2-124m-assets`](https://github.com/kamgueblondin/ai-os/releases/tag/gpt2-124m-assets).

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

QEMU nécessite **1 Gio** de RAM quand ce checkpoint est dans l’initrd. Dans le shell, utilisez `ai hello`, `ai-provider local`, `ai-model list`, `ai-runtime` et `rc`. La génération est volontairement bornée à 64 jetons de contexte et quatre jetons nouveaux. Sous QEMU TCG sans KVM, le temps d’une courte génération reste de l’ordre de **7 à 9 secondes** : l’objectif inférieur à une seconde n’est pas atteint.

## GGUF, OpenAI et réseau : limites assumées

AI-OS valide la structure de fichiers GGUF v3 et fournit des primitives Q8_0×FP32, mais les kernels Q3_K/Q4_K/Q6_K requis pour exécuter les checkpoints GPT-2 quantifiés ne sont pas encore présents. `ai-model` peut mémoriser un profil `.gguf`, sans le faire exécuter.

Le profil `ai-provider openai` est un **stub contrôlé**. `net-status` affiche l’absence de pilote Ethernet, ARP, IPv4, DHCP, DNS, TCP et TLS ; une commande `ai` avec ce profil ne transmet aucune requête et annonce l’indisponibilité. QEMU peut connecter une carte ISA ou PCI émulée à un backend hôte, mais cette possibilité ne remplace pas un pilote ni une pile protocolaire dans le noyau invité [1]. Les secrets OpenAI ne doivent jamais être inclus dans l’image, les logs série ou le dépôt.

## Tests et artefacts

`make test-all` a validé **182/182** tests : PMM (17), syscall (48), tâches (21), overlay (8), tokenizer (15), GGUF (5), quantification (5), échantillonnage GPT-2 (4), IPC (6), protocole VFS (6), registre de services (9), shell (25), RAMFS (10) et robustesse GGUF (3). `make integration-qemu` ajoute six validations QEMU séparées, dont les contrats IPC, médiateur VFS corrélé, découverte nommée, cycle de vie et transfert de propriété Foundation, et réinitialise son disque de contrat sans toucher à `build/overlay.img`.

Une ISO BIOS/GRUB peut être produite avec l’initrd. Lorsque les poids GPT-2 sont fournis, ils sont bien incorporés à l’ISO pour un fonctionnement local sur une machine vierge ; ils restent ignorés par Git.

## Roadmap du prototype

Le backlog courant est [US/ai_os_us.md](US/ai_os_us.md). La vision MOHHOS est conservée séparément dans [US/README.md](US/README.md).

- [x] GPT-2 local, cache KV, SSE2 et top-k borné
- [x] Tokenizer BPE UTF-8 avec couverture de lettres Unicode ciblée
- [x] Sonde GGUF v3 et primitives Q8_0
- [x] Tests QEMU versionnés
- [x] Overlay ATA PIO V2, 64 nœuds et restauration V1
- [x] Préemption IRQ0 sûre entre tâches Ring 3
- [x] Stub OpenAI/network honnête et testable
- [x] IPC Foundation non bloquant entre tâches Ring 3, avec identité d’émetteur noyau
- [x] Médiateur VFS Ring 3, lecture bornée et réponse IPC structurée
- [x] Registre de services nommé et cycle de vie : retrait propriétaire, nettoyage sur `exit`/`kill`
- [x] Corrélation requête-réponse locale : `request_id` IPC, réponse VFS filtrée et contrat QEMU
- [x] Transfert limité de publication : propriétaire, bénéficiaire Ring 3 et nettoyage après `kill`
- [ ] Capabilities, révocation, identité vérifiée, conservation des réponses discordantes et externalisation d’un backend VFS
- [ ] Migration microkernel réelle
- [ ] Inference GGUF quantifiée et latence QEMU inférieure à une seconde
- [ ] Pilote NIC, DHCP, DNS, TCP, TLS et client OpenAI effectif
- [ ] Système de fichiers disque général (ext2/FAT)

## Arborescence

```text
ai-os/
├── boot/                 # Multiboot et stubs ISR
├── kernel/               # mémoire, interruptions, tâches, syscalls et LLM
├── fs/                   # initrd TAR et overlay ATA
├── userspace/            # shell et programmes Ring 3
├── tests/                # Unity, robustesse et contrats QEMU
├── models/               # actifs locaux ignorés par Git
├── docs/                 # état réel et guides
└── US/                   # backlog prototype et archives MOHHOS
```

## Contribution

```bash
make deps && make ci
make integration-qemu
```

Les contributions ne doivent pas inclure `build/`, les ELF utilisateurs, les images ISO ou les modèles. Le code et la documentation du prototype sont en français.

## Références

[1] [QEMU, *Network emulation*](https://www.qemu.org/docs/master/system/devices/net.html)
