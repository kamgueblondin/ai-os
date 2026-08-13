# User Stories AI-OS (implémentation actuelle)

**Date :** 13 août 2026  
**Source de vérité runtime :** [docs/ETAT_REEL.md](../docs/ETAT_REEL.md)  
**Rôle :** backlog du **prototype i386** qui tourne sous QEMU. Ce n'est pas le plan MOHHOS (120 US, 8 phases).

Légende : **fait** = observable dans le code et vérifié par `make test-all` / `make qemu-smoke` (GPT-2 : aussi `make gpt2-recovery` si les poids sont présents). **suite** = prochaine tranche cohérente, pas une vision à 6 ans.

---

## Fait (noyau et shell)

### AOS-001 — Boot Multiboot sous QEMU
**En tant que** développeur, **je veux** un binaire Multiboot i386 qui démarre dans QEMU, **afin de** travailler sur un hobby OS réel.

Fait : `boot/boot.s`, VGA + série, `make run` / `make run-gui`. Pas d'UEFI.

### AOS-002 — Mémoire physique, virtuelle et tas
**En tant que** noyau, **je veux** allouer des pages et un tas, **afin d'** héberger tâches, initrd et (optionnellement) GPT-2.

Fait : PMM / VMM / heap, paging. `SYS_MEMINFO` (`mem`). Pas de prédiction IA des ressources (US-002 MOHHOS).

### AOS-003 — Interruptions, timer et clavier
**En tant que** utilisateur du shell, **je veux** taper des commandes au clavier PS/2, **afin d'** interagir avec le système.

Fait : PIC 8259, PIT 100 Hz, i8042, EOI IRQ0 **avant** `schedule()` (sinon IRQ1 bloquée). Le round-robin à chaque tick est **volontairement off** après le premier saut vers le shell.

### AOS-004 — Initrd TAR en lecture seule
**En tant que** shell, **je veux** lister et lire des fichiers empaquetés, **afin d'** avoir un disque racine au boot.

Fait : TAR POSIX, `SYS_LISTDIR` / `SYS_READFILE`. Programmes : `shell`, `idle`, `ok`, `fake_ai`, `ai_assistant`, `user_program`.

### AOS-005 — Shell ELF en Ring 3
**En tant que** utilisateur, **je veux** un prompt interactif isolé du noyau, **afin de** lancer des commandes sans rester dans une boucle kernel.

Fait : `userspace/shell.c`, prompt `/ (-.-) :`, `jump_to_task()`. Builtins : voir le tableau dans ETAT_REEL.

### AOS-006 — ABI de syscalls
**En tant que** programme userspace, **je veux** un ensemble d'appels `int 0x80` documenté, **afin de** parler au noyau sans bidouiller.

Fait : `include/os_syscalls.h`, numéros 0-22 (`MAX_SYSCALLS` = 23), dont overlay, tâches et `SYS_GPT2_GENERATE`.

### AOS-007 — Overlay RAM persisté
**En tant que** utilisateur, **je veux** créer et modifier de petits fichiers qui survivent à un reboot QEMU, **afin de** ne pas tout perdre à chaque `make run`.

Fait : 32 nœuds, fichiers ≤ 256 octets, snapshot ATA PIO LBA28 (`AIOV`) sur le maître IDE. Pas d'ext2/FAT.

### AOS-008 — Tâches coopératives
**En tant que** utilisateur, **je veux** `spawn` / `yield` / `ps` / `kill`, **afin de** voir une deuxième tâche tourner sans casser le clavier.

Fait : bascule depuis le cadre user de `int 0x80`. `idle` affiche `idle ok` puis yield. Pas de préemption IRQ0 continue.

### AOS-009 — Exec bloquant
**En tant que** shell, **je veux** lancer un ELF et attendre sa fin, **afin de** récupérer un code de retour.

Fait : parent `TASK_WAITING`, enfant `SYS_EXIT` réveille le waiter. `ok` → `exec ok` → `rc ok 0`.

### AOS-010 — Inférence GPT-2 locale
**En tant que** utilisateur, **je veux** `ai <texte>` hors ligne, **afin d'** obtenir une complétion courte sans réseau.

Fait si `models/gpt2_124M.bin` + `gpt2_tokenizer.bin` sont dans l'initrd : `SYS_GPT2_GENERATE`, cache KV, SSE2, top-k (k=8, température 0,6), pénalité **uniquement sur les jetons déjà émis**. 64 jetons de prompt, 12 de sortie. Pas TensorFlow Lite, pas GGUF, pas OpenAI effectif. Sans poids : message d'indisponibilité (le shell reste utilisable).

### AOS-011 — Tokenizer BPE
**En tant que** moteur GPT-2, **je veux** encoder le prompt et décoder les jetons en octets tiktoken, **afin que** la sortie ne soit pas une suite d'espaces artificiels.

Fait : fusions par plus petit id vocabulaire, decode brut (espace, UTF-8). Regex unicode `\p{L}` complet : non.

### AOS-012 — Tests et CI
**En tant que** contributeur, **je veux** un gate reproductible, **afin de** ne pas casser le shell à chaque PR.

Fait : Unity **144** (`test_pmm` 17, `test_syscall` 48, `test_task` 21, `test_overlay` 6, `test_tokenizer` 13, `test_gpt2_sample` 4, `test_shell` 25, `test_ramfs` 10). `make qemu-smoke` : cinq boots. GitHub Actions = `make ci`. Dossiers `tests/integration`, `system`, `performance`, `robustness` : **vides**.

---

## Suite cohérente (prochaines tranches)

Ordre volontaire : rester sur i386 / QEMU / Ring 3. Ne pas commencer un microkernel, du P2P ou PromptMessage tant que le prototype n'a pas ces briques.

| ID | User story | Pourquoi maintenant |
|---|---|---|
| AOS-020 | Quantification / GGUF / latence &lt; 1 s sous QEMU | GPT-2 marche, ~7,7 s pour 4 jetons |
| AOS-021 | BPE `\p{L}` (unicode lettres) | L'encodeur ASCII+UTF-8 chunké limite les prompts |
| AOS-022 | Tests d'intégration non vides (scripts QEMU déjà là) | Unity ne couvre pas `ai` sans poids |
| AOS-023 | FS disque général (ext2 ou FAT) au-delà du snapshot 32 nœuds | L'overlay ATA est un cache, pas un FS |
| AOS-024 | Préemption round-robin IRQ0 **sans** tuer IRQ1 | Coopératif seulement, par stabilité clavier |
| AOS-025 | Pile réseau minimale (ou stub OpenAI documenté comme absent) | `ai-provider openai` est un profil vide |

Hors suite proche (vision MOHHOS, pas le backlog courant) : microkernel, apprentissage fédéré, navigateur-OS, PromptMessage, P2P, multi-plateforme, économie de points. Voir [README.md](README.md) et [individual_us/INDEX.md](individual_us/INDEX.md).
