# Volume FAT sur disque IDE — conception

**Statut :** conception du prochain stockage disque. **Pas livré.**

**Date :** 16 août 2026.

**Source de vérité runtime :** [ETAT_REEL.md](ETAT_REEL.md). Vocabulaire : [vocabulaire.md](vocabulaire.md). Backlog : [../US/ai_os_us.md](../US/ai_os_us.md) (AOS-026).

## Décision

Le premier système de fichiers **sur disque**, hors overlay AIOV, est un **volume FAT** (FAT16 en premier jalon, FAT12 acceptable pour une image minuscule). **ext2, ext3, ext4 et tout système à inodes sont hors périmètre.**

AI-OS n’est pas un clone Unix. Un volume à table d’allocation (clusters, répertoire d’entrées fixes) suffit pour persister des fichiers plus grands que l’overlay, sans importer le modèle inode / superbloc / liens physiques.

## Ce qui existe déjà (à ne pas confondre)

| Support | Format | Rôle | Limite |
|---|---|---|---|
| Initrd | Archive TAR (ustar) en RAM | Programmes, modèles, lecture seule | Pas un volume disque |
| Overlay | Snapshot **AIOV** V2 sur ATA PIO | Petits fichiers persistants du shell | 64 nœuds, 80 octets de chemin, 384 octets de contenu, 64 secteurs (32 Kio) à LBA 0 |
| Disque IDE QEMU | Image brute `build/overlay.img` | Porte uniquement le snapshot AIOV | Pas de table d’allocation, pas de répertoire sur disque |

L’overlay occupe les **64 premiers secteurs** (LBA 0–63). Un volume FAT ne doit **pas** les écraser. Deux placements possibles, dans cet ordre de préférence :

1. **Second disque IDE** dédié (`-drive …,if=ide`), image FAT préparée à la construction.
2. **Même disque**, volume FAT à partir d’un LBA réservé (par exemple 64), une fois l’image agrandie.

Le jalon AOS-026 doit choisir l’une des deux et la documenter dans les tests QEMU. Le médiateur `vfsserver` pourra plus tard exposer un préfixe `fat/` ; le pilote de volume reste une affaire de secteurs et de clusters, pas un « VFS Linux ».

## Pourquoi FAT, et pas un FS Unix

- **Modèle simple :** secteur d’amorçage (BPB), une ou deux tables d’allocation, répertoire racine, chaînes de clusters. Pas d’inodes, pas de groupes de blocs, pas de journal.
- **Taille adaptée à ATA PIO LBA28 :** une image de quelques Mio se parcourt secteur par secteur sans cache de blocs sophistiqué.
- **Préparation hors de l’OS :** une image FAT se fabrique sur la machine de build, puis QEMU la présente comme disque IDE. AI-OS n’a pas besoin d’un outil « type mkfs » calqué sur un Unix.
- **Distance volontaire :** FAT vient des volumes DOS/Windows. Ce n’est pas une invitation à recopier Linux ; c’est un format de volume lisible et borné.

FAT32 (FAT étendue, FSInfo, clusters 32 bits) est reporté. Le premier jalon est **FAT16** sur une image de 2 à 16 Mio, secteurs de 512 octets, 1 ou 2 tables.

## Contrat du premier jalon (AOS-026)

**En tant qu’**utilisateur, **je veux** lire (puis écrire) des fichiers sur un volume FAT branché en IDE, **afin de** dépasser le snapshot AIOV sans changer d’identité de système.

| Livrable | Critère de sortie |
|---|---|
| Lecture du BPB | Reconnaître FAT16 (ou FAT12), octets par secteur = 512, refuser un volume illisible |
| Table d’allocation | Suivre une chaîne de clusters jusqu’à la sentinelle de fin |
| Répertoire racine | Lister des entrées 8.3 ; ignorer volume label et entrées effacées |
| Lecture de fichier | Reconstituer le contenu depuis les clusters, bornée (au moins 4 Kio au premier test) |
| Persistance | Un fichier présent sur l’image **avant** le boot est visible après `make run` |
| Isolation overlay | Les 64 secteurs AIOV restent intacts ; les tests overlay existants restent verts |
| Hors jalon | Noms longs (LFN), FAT32, sous-répertoires profonds, fragmentation malveillante, journal, droits, exécutable depuis FAT |

L’écriture (création, allongement, suppression) est une **tranche suivante** du même volume, pas un second format.

## Place dans la suite du prototype

| Priorité proche | Rapport avec FAT |
|---|---|
| Kernels GGUF Q3_K / Q4_K / Q6_K | Indépendant. Un GGUF trop gros pour l’initrd pourra plus tard vivre sur le volume FAT. |
| Latence GPT-2 &lt; 1 s | Indépendant. |
| Externaliser le backend du médiateur | Le volume FAT est un **nouveau backend** possible ; il ne retire pas encore initrd/overlay du noyau. |
| Pilote NIC → IP → TLS | Orthogonal. Voir [aos025_network_stub.md](aos025_network_stub.md). |

Ce n’est **pas** le moment d’introduire ext2 « pour faire comme un Unix », ni de prétendre qu’AI-OS a un système de fichiers généraliste.

## Vérification prévue

```text
make test-all              # parseur BPB, chaînes, refus de volume invalide
make qemu-smoke            # overlay AIOV inchangé
make integration-qemu      # contrat : ls/cat d’un fichier préparé sur l’image FAT
```

Aucun secret, aucun modèle et aucune clé API ne doivent être committés sur l’image FAT de test.
