# AOS-1289 à AOS-1304 — création transactionnelle de fichier FAT32

> **État :** implémenté ; validation historique du lot : **419 tests verts**. La validation courante est maintenue dans `docs/todo.md`.

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
| Validation au moment du lot | 419 tests verts |

La création de fichier reste volontairement limitée à un alias 8.3 et séparée de la recherche par nom long. Depuis ce lot, l’extension de la racine et les primitives LFN FAT32 bornées ont été livrées ; la publication multi-entrée, la reconstruction LFN et l’intégration VFS restent à réaliser.

**Auteur :** Manus AI
