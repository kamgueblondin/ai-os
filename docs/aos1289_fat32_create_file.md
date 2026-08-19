# AOS-1289 à AOS-1304 — création transactionnelle de fichier FAT32

> **État :** implémenté et validé localement. **Suite globale : 419/419 tests verts.**

`fat32_create_file` orchestre les primitives FAT32 caller-owned. Il valide l’alias 8.3, calcule le nombre de clusters requis, réserve chaque cluster avec un marqueur EOC, relie les clusters successifs, remplit un buffer statique borné à 128 secteurs, écrit chaque cluster puis publie l’entrée racine seulement après succès de toutes les écritures.

En cas d’échec d’allocation, d’écriture, de chaînage ou de publication, le chemin de rollback parcourt la chaîne déjà réservée avec une garde bornée par le nombre de clusters du volume et remet les entrées FAT à zéro dans toutes les copies. L’appelant conserve la propriété du buffer source ; le buffer statique interne n’est qu’un tampon de secteurisation et ne contient pas d’état de fichier persistant.

| Propriété | Garantie |
|---|---|
| Données | buffer source caller-owned |
| Taille de cluster | jusqu’à 128 secteurs de 512 octets |
| Publication | après écriture complète |
| Rollback | libération FAT de la chaîne partielle |
| Nom | 8.3 ASCII, sans LFN |
| Allocation dynamique | aucune |
| Tests noyau | 35/35 |
| Suite globale | 419/419 |

Le prochain incrément pourra ajouter l’extension automatique du répertoire FAT32, puis les entrées LFN FAT32. L’écriture de fichier est volontairement séparée de la recherche par nom long et des syscalls de montage afin de préserver le contrat ABI actuel.

**Auteur :** Manus AI
