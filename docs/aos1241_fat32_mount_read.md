# AOS-1241 à AOS-1256 — montage et lecture FAT32

> **État :** implémenté et validé localement. **Suite globale : 419/419 tests verts.**

Ce macro-lot ajoute un volume FAT32 distinct du volume FAT16, sans modifier la structure FAT16 ni ses offsets 16 bits. Le montage valide le BPB : 512 octets par secteur, secteurs par cluster puissance de deux, région réservée non nulle, une ou deux FAT, `RootEntCnt = 0`, `FATSz16 = 0`, `FATSz32` non nul, signature de boot et cluster racine valide. Le nombre de clusters doit se situer dans la plage FAT32.

La lecture d’une entrée FAT utilise quatre octets et masque les quatre bits réservés pour obtenir une valeur 28 bits. `fat32_cluster_lba` calcule la région de données sans dépassement de la racine spéciale FAT32. `fat32_read_cluster` lit directement les secteurs consécutifs dans un buffer caller-owned ; aucun buffer de fichier, chaîne ou répertoire n’est alloué implicitement.

| Élément | Contrat |
|---|---|
| Montage | `fat32_mount(volume, read_sector, base_lba)` |
| Entrée FAT | `fat32_read_fat_entry(volume, cluster, out_next)` |
| Adresse données | `fat32_cluster_lba(volume, cluster, out_lba)` |
| Lecture cluster | `fat32_read_cluster(volume, cluster, buffer)` |
| Allocation dynamique | Aucune |
| Écriture FAT32 | Non incluse dans ce lot |
| Tests noyau | 35/35 |
| Suite globale | 419/419 |

La création de fichiers, la réplication d’écritures dans les deux FAT, les entrées LFN FAT32 et les syscalls de montage restent des incréments séparés. Cette séparation évite de réutiliser les primitives FAT16 dont les index de cluster et la racine fixe ne sont pas compatibles FAT32.

**Auteur :** Manus AI
