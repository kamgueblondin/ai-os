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

## Publication et reconstruction livrées

`fat32_create_lfn_file` réserve d’abord les données via le créateur FAT32 transactionnel, localise l’alias 8.3, libère son slot logique puis publie les ordinals LFN décroissants dans les slots suivants avant de réécrire l’alias court. Les slots traversent la chaîne du répertoire racine et peuvent provoquer son extension automatique ; les buffers restent caller-owned et aucune allocation dynamique n’est introduite.

`fat32_list_root` parcourt la chaîne racine, ignore les entrées supprimées et les volumes, reconstitue les fragments UTF-16LE ASCII dans un buffer `os_fat16_dirent_t` fourni par l’appelant, puis n’accepte le nom long que si la séquence d’ordinals et le checksum de l’alias sont cohérents. En cas de séquence invalide, le listage retombe sur l’alias 8.3 plutôt que de publier un nom non vérifié.

## Limites explicites

Le contrat reste borné à l’ASCII, à `OS_NAME_MAX - 1` caractères et à 20 entrées LFN par fichier. Les caractères Unicode hors ASCII, les paires substituts UTF-16 complètes, la suppression/renommage LFN et l’intégration complète au VFS FAT32 restent hors périmètre.

## Validation

Le test `test_fat32_lfn_encoding` vérifie checksum, ordinal, attribut, UTF-16LE, borne de 13 caractères et rejet du quatorzième caractère. `test_fat32_lfn_file_and_list` vérifie création d’un nom multi-entrée, publication de l’alias et reconstruction validée au listage. Le module FAT32 passe **4/4 tests** et la suite `make test-all` reste verte avec **421 tests exécutés avec succès**.
