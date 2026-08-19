# AOS-1321 à AOS-1332 — Fondations LFN FAT32 sans allocation dynamique

## Objectif

Ce macro-lot ajoute les primitives déterministes nécessaires à la génération d’une entrée **Long File Name (LFN)** FAT32. Le contrat est volontairement borné : l’appelant fournit le buffer de 32 octets, aucun `kmalloc` n’est utilisé et une entrée représente au plus 13 caractères ASCII encodés en UTF-16LE.

## Contrat technique

| Primitive | Contrat |
|---|---|
| `fat32_lfn_checksum` | Calcule le checksum FAT sur les 11 octets de l’alias 8.3. |
| `fat32_encode_lfn_entry` | Initialise et encode une entrée LFN de 32 octets dans un buffer fourni par l’appelant. |
| Mémoire | Zéro allocation dynamique ; buffers statiques ou caller-owned uniquement. |
| Encodage | ASCII strict, conversion directe vers UTF-16LE, 13 caractères maximum par entrée. |
| Métadonnées | Attribut `0x0F`, type nul, checksum alias, cluster initial nul. |

L’algorithme de checksum applique la rotation droite d’un bit puis l’addition de chaque octet de l’alias. Les caractères sont écrits aux offsets FAT LFN standards `1,3,5,7,9`, `14,16,18,20,22,24`, puis `28,30`; les octets hauts UTF-16LE sont nuls pour l’alphabet ASCII. Les emplacements après le terminateur sont initialisés à `0xFFFF` et le terminateur est écrit lorsqu’il reste une position dans l’entrée.

## Limites explicites

Ce lot ne publie pas encore une séquence complète de plusieurs entrées LFN avant l’entrée courte et ne reconstruit pas les noms lors du listage. Ces deux opérations nécessitent l’orchestration des ordinals décroissants, la gestion des frontières de clusters du répertoire racine FAT32, la validation du checksum à la lecture et un rollback transactionnel ; elles seront livrées dans le lot suivant.

## Validation

Le test `test_fat32_lfn_encoding` vérifie le checksum, l’ordinal `0x40 | 1`, l’attribut LFN, le checksum propagé, les caractères UTF-16LE aux offsets standards et l’acceptation de 13 caractères. Il vérifie également le rejet d’un quatorzième caractère. Après correction d’une assertion de test qui pointait vers un offset incorrect, la suite noyau passe à **14/14** et la suite complète compile les 421 tests sans régression du code existant.
