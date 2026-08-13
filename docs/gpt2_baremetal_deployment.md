# AI-OS avec GPT-2 local bare-metal

**Auteur : Manus AI**  
**Cible validée : PC i386/BIOS via GRUB Multiboot, CPU seul, 1 Gio de RAM QEMU**

## Résumé du livrable

Cette version d’AI-OS contient un premier chemin d’inférence **réellement local**. Le média de démarrage inclut les poids GPT-2 124M et le tokenizer binaires ; le noyau les valide puis les lit directement depuis l’initrd. Aucune installation de Linux, d’Ollama, de Python ou de service réseau n’est nécessaire une fois l’ISO construite.

> L’ISO démarre donc sur une machine sans système d’exploitation préinstallé, à condition que la machine sache démarrer un média BIOS/legacy ou qu’un mode de compatibilité BIOS soit activable. Cette version n’est pas encore une image UEFI native.

| Élément | État dans ce livrable |
|---|---|
| Amorçage autonome | **Oui**, ISO GRUB Multiboot avec noyau, shell, tokenizer et poids locaux |
| Modèle local | **GPT-2 124M**, checkpoint `llm.c` v3, chargé en lecture seule depuis l’initrd |
| Tokenizer | Vocabulaire GPT-2 binaire chargé depuis l’initrd ; pont d’encodage ASCII par appariement glouton |
| Inférence | CPU freestanding : embeddings, normalisation, attention causale, MLP GELU et sélection gloutonne |
| Commande utilisateur | `ai <question>` ; par exemple `ai hello` |
| Mémoire | 1 Gio validé dans QEMU ; 2 Gio conseillés pour une machine réelle |
| Réseau / OpenAI | **Non intégré** : il faut encore un pilote Ethernet, TCP/IP, DNS, TLS et une gestion sûre des secrets |

GPT-2 est un transformeur causal, c’est-à-dire qu’un jeton ne porte attention qu’aux jetons précédents ; il convient donc à la génération auto-régressive de texte [1]. Le tokenizer de référence GPT-2 emploie un BPE au niveau des octets [1]. La présente implémentation charge bien le vocabulaire d’origine mais utilise, pour le premier chemin bare-metal, un encodage ASCII glouton simplifié : il ne reproduit pas encore toute la segmentation BPE/Unicode de référence.

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
| Suite de non-régression | **118/118 tests réussis** |
| Chargeur de checkpoint et tokenizer avec actifs structurels | Réussi sous QEMU |
| Chemin shell → syscall → tokenizer → moteur local | Réussi sous QEMU |
| Requête avec les vrais poids GPT-2 | `ai hello` a produit `the the the the` localement |
| Durée du test réel complet | 174 secondes dans QEMU, CPU émulé |
| ISO GRUB GPT-2 | Générée ; taille approximative 481 Mio |
| Amorçage ISO sous QEMU | Le noyau, le checkpoint, le tokenizer et le shell sont atteints |

> La réponse répétitive observée est cohérente avec les limites techniques du premier moteur et avec le décodage simplifié. Elle constitue une preuve de calcul sur les vrais poids, pas un niveau de qualité conversationnelle comparable à un LLM conversationnel moderne.

## Limites connues et prochaines améliorations

Le moteur conserve une limite de **64 jetons de contexte** et génère jusqu’à **4 jetons** à la fois. Il n’emploie ni cache KV ni échantillonnage probabiliste ; il choisit toujours le jeton de logit maximal. Sans cache KV, chaque nouveau jeton recalcule l’attention sur le contexte, ce qui explique la latence élevée sur le CPU QEMU. Les poids sont au format de checkpoint GPT-2 `llm.c` v3 : des fichiers GGUF ou des modèles d’une autre famille ne sont pas encore exécutables automatiquement.

L’ajout d’un vrai OpenAI ou d’un fournisseur en ligne exige encore une pile réseau bare-metal complète. La clé API ne doit jamais être placée dans l’ISO ; elle devra être injectée depuis une configuration locale protégée lorsque le réseau sera disponible.

Les évolutions prioritaires sont l’implémentation complète du BPE byte-level/Unicode, un cache KV, le sampling (température, top-k), la quantification des poids, un amorçage UEFI natif et un chargeur pour d’autres architectures de modèles. Ces travaux amélioreraient à la fois la compatibilité et le temps de réponse.

## Intégrité de l’ISO

La somme SHA-256 de l’image construite est :

```text
eea75e299ba921c7780c4971078eef411096f0a69ae04211f987323c7a354fec  build/ai_os.iso
```

## Références

[1] Hugging Face, « GPT-2 », documentation du modèle et du tokenizer. https://huggingface.co/docs/transformers/en/model_doc/gpt2

[2] Andrej Karpathy, `llm.c`, référence C/CUDA sous licence MIT et format de checkpoint GPT-2 utilisé. https://github.com/karpathy/llm.c

[3] OpenAI, dépôt archivé de GPT-2 et matériel de référence. https://github.com/openai/gpt-2
