# AI-OS avec GPT-2 local bare-metal

**Auteur : Manus AI**  
**Cible validée : PC i386/BIOS via GRUB Multiboot, CPU seul, 1 Gio de RAM QEMU**

## Résumé du livrable

Cette version d’AI-OS contient un premier chemin d’inférence **réellement local**. Lorsque les poids GPT-2 124M et le tokenizer binaires sont placés dans `models/` avant la construction, le média de démarrage les inclut et le noyau les valide puis les lit directement depuis l’initrd. Aucune installation de Linux, d’Ollama, de Python ou de service réseau n’est nécessaire une fois cette ISO construite.

> L’ISO démarre donc sur une machine sans système d’exploitation préinstallé, à condition que la machine sache démarrer un média BIOS/legacy ou qu’un mode de compatibilité BIOS soit activable. Cette version n’est pas encore une image UEFI native.

| Élément | État dans ce livrable |
|---|---|
| Amorçage autonome | **Oui**, ISO GRUB Multiboot avec noyau, shell, tokenizer et poids locaux lorsque les artefacts sont fournis au build |
| Modèle local | **GPT-2 124M**, checkpoint `llm.c` v3, chargé en lecture seule depuis l’initrd |
| Tokenizer | Vocabulaire GPT-2 binaire chargé depuis l’initrd ; pont d’encodage ASCII par appariement glouton |
| Inférence | CPU freestanding : embeddings, normalisation, attention causale, MLP GELU, cache KV et échantillonnage top-k |
| Commande utilisateur | `ai <question>` ; par exemple `ai hello` |
| Mémoire | 1 Gio validé dans QEMU ; 2 Gio conseillés pour une machine réelle |
| Réseau / OpenAI | **Non intégré** : il faut encore un pilote Ethernet, TCP/IP, DNS, TLS et une gestion sûre des secrets |

GPT-2 est un transformeur causal, c’est-à-dire qu’un jeton ne porte attention qu’aux jetons précédents ; il convient donc à la génération auto-régressive de texte [1]. Le tokenizer de référence GPT-2 emploie un BPE au niveau des octets [1]. La présente implémentation charge bien le vocabulaire d’origine mais utilise, pour le premier chemin bare-metal, un encodage ASCII glouton simplifié : il ne reproduit pas encore toute la segmentation BPE/Unicode de référence.

## Obtenir et vérifier les artefacts GPT-2

Les poids ne sont pas stockés dans l’historique Git. Téléchargez `gpt2_124M.bin`, `gpt2_tokenizer.bin` et `gpt2-124m-assets.sha256` depuis la [release publique GPT-2 124M](https://github.com/kamgueblondin/ai-os/releases/tag/gpt2-124m-assets), puis placez-les dans `models/`.

```bash
mkdir -p models
curl -L -o models/gpt2_124M.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_124M.bin
curl -L -o models/gpt2_tokenizer.bin https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2_tokenizer.bin
curl -L -o models/gpt2-124m-assets.sha256 https://github.com/kamgueblondin/ai-os/releases/download/gpt2-124m-assets/gpt2-124m-assets.sha256
(cd models && sha256sum -c gpt2-124m-assets.sha256)
```

Les empreintes attendues sont les suivantes :

| Fichier | SHA-256 |
|---|---|
| `gpt2_124M.bin` | `3da8b207584030bcdcd207cf7a99952e3421dce92da218b351071857511bf162` |
| `gpt2_tokenizer.bin` | `6f3abc21e444e4e8300e225f4e03da48ea121cf17e30f67009b8dad7a66c2f13` |

## Contenu de l’ISO

L’archive `build/ai_os.iso` contient les éléments suivants :

| Composant | Emplacement dans l’ISO | Rôle |
|---|---|---|
| Noyau AI-OS | `/boot/ai_os.bin` | Noyau i386 Multiboot et gestion mémoire étendue |
| Initrd | `/boot/my_initrd.tar` | Shell, programmes utilisateurs, poids et vocabulaire |
| Poids GPT-2 | `/models/gpt2_124M.bin` dans l’initrd | Checkpoint binaire de 497 904 640 octets |
| Tokenizer GPT-2 | `/models/gpt2_tokenizer.bin` dans l’initrd | Vocabulaire de 50 257 jetons |
| Configuration GRUB | `/boot/grub/grub.cfg` | Amorçage direct du noyau et du module initrd |

Les actifs GPT-2 employés suivent le format de checkpoint CPU version 3 documenté par la référence `llm.c`, qui publie explicitement le chargement d’un fichier `gpt2_124M.bin` et d’un tokenizer binaire associé [2].

## Démarrage sur une machine vierge

Copiez l’ISO sur une clé USB avec un outil d’écriture d’image, par exemple Balena Etcher, Rufus en mode image DD, ou GNOME Disks. Démarrez ensuite la machine sur cette clé. L’image est une image **BIOS/legacy** ; sur une machine UEFI stricte sans CSM/legacy boot, elle ne démarrera pas encore.

Après le démarrage, le shell AI-OS apparaît. Les commandes suivantes sont disponibles :

| Commande | Résultat |
|---|---|
| `ai hello` | Envoie le prompt au moteur GPT-2 local |
| `ai-model list` | Affiche GPT-2 comme modèle local opérationnel |
| `ai-model use gpt2_124M.bin` | Sélectionne le profil GPT-2 chargé à l’amorçage |
| `ai-runtime` | Affiche les capacités et limites réelles du moteur |
| `ai-provider local` | Force le fournisseur local |

## Validations réalisées

Les validations suivantes ont été exécutées dans le bac à sable :

| Vérification | Résultat |
|---|---|
| Compilation complète d’AI-OS | Réussie |
| Suite de non-régression | **121/121 tests réussis** |
| Chargeur de checkpoint et tokenizer avec actifs structurels | Réussi sous QEMU |
| Chemin shell → syscall → tokenizer → moteur local | Réussi sous QEMU |
| Requête avec les vrais poids GPT-2 | Sortie locale produite sous le préfixe `[GPT-2 local]` |
| Reprise après une réponse | `rc` accepté par le shell dans un test QEMU dédié |
| Latence observée | **7,693 s** pour `ai hello` et quatre jetons, QEMU Pentium III SSE2 sans KVM |
| ISO GRUB GPT-2 | Générée ; taille approximative 481 Mio |
| Amorçage ISO sous QEMU | Le noyau, le checkpoint, le tokenizer et le shell sont atteints |

> La qualité conversationnelle reste limitée par le modèle GPT-2 124M, la longueur de sortie volontairement courte et l’encodage ASCII simplifié. Une sortie générée localement confirme le chemin de calcul, sans prétendre atteindre le niveau d’un LLM instructionnel moderne.

## Limites connues et prochaines améliorations

Le moteur conserve une limite de **64 jetons de contexte** et génère jusqu’à **4 jetons** à la fois. Il utilise un cache KV et un échantillonnage top-k avec pénalité de répétition, ce qui évite de recalculer le préfixe pour chaque jeton. La compilation SSE2 emploie aussi `-mstackrealign`, nécessaire pour garantir l’alignement des opérations vectorielles dans les chemins noyau. Cependant, chaque nouveau jeton exécute encore les projections d’attention, le MLP et la projection de vocabulaire complète en FP32 ; l’émulation QEMU sans KVM reste donc coûteuse. Les poids sont au format de checkpoint GPT-2 `llm.c` v3 : des fichiers GGUF ou des modèles d’une autre famille ne sont pas encore exécutables automatiquement.

L’ajout d’un vrai OpenAI ou d’un fournisseur en ligne exige encore une pile réseau bare-metal complète. La clé API ne doit jamais être placée dans l’ISO ; elle devra être injectée depuis une configuration locale protégée lorsque le réseau sera disponible.

Les évolutions prioritaires sont l’implémentation complète du BPE byte-level/Unicode, la quantification INT8 ou INT4 des poids, des kernels SIMD supplémentaires, un amorçage UEFI natif et un chargeur pour d’autres architectures de modèles. Ces travaux amélioreraient à la fois la compatibilité et le temps de réponse.

## Intégrité de l’ISO

L’ISO dépend des artefacts GPT-2 fournis localement et n’a donc pas de somme SHA-256 universelle dans le dépôt. Après construction, calculez et archivez la somme correspondant à vos fichiers de modèle :

```bash
sha256sum build/ai_os.iso
```

## Références

[1] Hugging Face, « GPT-2 », documentation du modèle et du tokenizer. https://huggingface.co/docs/transformers/en/model_doc/gpt2

[2] Andrej Karpathy, `llm.c`, référence C/CUDA sous licence MIT et format de checkpoint GPT-2 utilisé. https://github.com/karpathy/llm.c

[3] OpenAI, dépôt archivé de GPT-2 et matériel de référence. https://github.com/openai/gpt-2
