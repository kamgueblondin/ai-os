# Volume FAT sur disque IDE - conception

**Statut :** jalon AOS-026 lecture seule livre, puis mutations FAT16 et FAT32 8.3 racine via VFS. FAT32 reste un second disque IDE.

**Date :** 23 aout 2026.

**Source de verite runtime :** [ETAT_REEL.md](ETAT_REEL.md). Vocabulaire : [vocabulaire.md](vocabulaire.md). Backlog : [../US/ai_os_us.md](../US/ai_os_us.md) (AOS-026).

## Decision

Le premier systeme de fichiers **sur disque**, hors overlay AIOV, est un **volume FAT** (FAT16 en premier jalon, FAT12 acceptable pour une image minuscule). **ext2, ext3, ext4 et tout systeme a inodes sont hors perimetre.**

AI-OS n'est pas un clone Unix. Un volume a table d'allocation (clusters, repertoire d'entrees fixes) suffit pour persister des fichiers plus grands que l'overlay, sans importer le modele inode / superbloc / liens physiques.

## Ce qui existe deja (a ne pas confondre)

| Support | Format | Role | Limite |
|---|---|---|---|
| Initrd | Archive TAR (ustar) en RAM | Programmes, modeles, lecture seule | Pas un volume disque |
| Overlay | Snapshot **AIOV** V2 sur ATA PIO | Petits fichiers persistants du shell | 64 noeuds, 80 octets de chemin, 384 octets de contenu, 64 secteurs (32 Kio) a LBA 0 |
| Disque IDE principal | Image brute `build/overlay.img` | Snapshot AIOV aux LBA 0-63, volume FAT16 a partir du LBA 64 | FAT16 VFS : create/remove/rename 8.3 racine seulement |
| Second disque IDE | Image FAT32 de fixture | Lecture et mutations 8.3 racine via `fat32/` | Pas de LFN VFS, pas de pretention FS generaliste |

L'overlay occupe les **64 premiers secteurs** (LBA 0-63). Un volume FAT ne doit **pas** les ecraser.

1. **Second disque IDE** dedie (`-drive ...,if=ide`) pour FAT32.
2. **Meme disque**, volume FAT16 a partir du LBA 64, overlay AIOV intact.

## Commandes observables

- `fat16-list` et `fat16-cat <8.3>` : lecture directe du volume FAT16.
- `vfs-list fat16/`, `vfs-read fat16/<8.3>`, `vfs-stat fat16/<8.3>` : lecture mediee.
- `vfs-write fat16/<8.3> <texte>` : cree un fichier racine, refuse l'ecrasement.
- `vfs-remove fat16/<8.3>` : marque l'entree 8.3 supprimee et libere la chaine.
- `vfs-rename fat16/<ancien> fat16/<nouveau>` : renomme sans deplacer la chaine, refuse une cible existante.
- `vfs-mount-add media32/ fat32` puis `vfs-list` / `vfs-read` / `vfs-stat` : lecture FAT32.
- `vfs-write fat32/<8.3> <texte>` / `vfs-remove` / `vfs-rename` : mutations 8.3 racine, meme politique que FAT16.

Hors contrat actuel : LFN VFS, sous-repertoires, ecrasement, remplacement transactionnel.

## Pourquoi FAT, et pas un FS Unix

- **Modele simple :** secteur d'amorcage (BPB), une ou deux tables d'allocation, repertoire racine, chaines de clusters. Pas d'inodes, pas de groupes de blocs, pas de journal.
- **Taille adaptee a ATA PIO LBA28 :** une image de quelques Mio se parcourt secteur par secteur sans cache de blocs sophistique.
- **Preparation hors de l'OS :** une image FAT se fabrique sur la machine de build, puis QEMU la presente comme disque IDE.
- **Distance volontaire :** FAT vient des volumes DOS/Windows. Ce n'est pas une invitation a recopier Linux.

## Contrat du premier jalon (AOS-026) puis suite

**En tant qu'**utilisateur, **je veux** lire (puis ecrire de facon bornee) des fichiers sur un volume FAT branche en IDE, **afin de** depasser le snapshot AIOV sans changer d'identite de systeme.

| Livrable | Etat |
|---|---|
| Lecture du BPB FAT16 | Livre |
| Table d'allocation et racine 8.3 | Livre |
| Lecture de fichier chainee | Livre |
| Isolation overlay LBA 0-63 | Livre |
| Creation / suppression / renommage FAT16 8.3 racine via VFS | Livre sous capacite `mutate` |
| Creation / suppression / renommage FAT32 8.3 racine via VFS | Livre sous capacite `mutate` |
| LFN, sous-repertoires, ecrasement VFS | Hors perimetre |
| FAT32 lecture / listage / statut VFS | Livre sur le second disque |
| ext2 et assimilés | Hors perimetre |

## Place dans la suite du prototype

| Priorite proche | Rapport avec FAT |
|---|---|
| Kernels GGUF Q3_K / Q4_K / Q6_K | Un GGUF trop gros pour l'initrd vit deja sur FAT16 (`make gguf-disk`). |
| Latence GPT-2 < 1 s | Independante. QEMU TCG reste ~48 s / ~23 s. |
| Externaliser le backend du mediateur | FAT16/FAT32 sont des backends VFS ; ils ne retirent pas encore initrd/overlay du noyau. |
| TLS authentifie / HTTP / OpenAI | Orthogonal. Voir [aos025_network_stub.md](aos025_network_stub.md). |

Ce n'est **pas** le moment d'introduire ext2 "pour faire comme un Unix", ni de pretendre qu'AI-OS a un systeme de fichiers generaliste.

## Verification

```text
make test-all              # parseur BPB, chaines, mutations FAT16, refus de volume invalide
make qemu-smoke            # overlay AIOV inchange
make qemu-vfs-service      # FAT16 et FAT32 create/read/rename/remove
make integration-qemu      # contrats QEMU
```

Aucun secret, aucun modele et aucune cle API ne doivent etre committes sur l'image FAT de test.
