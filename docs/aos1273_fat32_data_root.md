# AOS-1273 à AOS-1288 — données et entrée racine FAT32

> **État :** implémenté et validé localement. **Suite globale : 419/419 tests verts.**

Ce macro-lot ajoute l’écriture complète d’un cluster FAT32 dans un buffer caller-owned et la publication d’une entrée racine 8.3. Le nom est validé sans chemin ni caractères interdits, puis normalisé en majuscules dans les onze octets 8.3. L’entrée stocke l’attribut, le cluster initial sur 28 bits réparti entre les champs haut et bas et la taille little-endian sur 32 bits.

La recherche parcourt la chaîne du répertoire racine FAT32, secteur par secteur, et réutilise le premier slot libre ou supprimé. Si la chaîne se termine sans slot disponible, aucune allocation implicite n’est réalisée : l’appelant devra allouer et chaîner un nouveau cluster de répertoire dans un incrément ultérieur. L’écriture du cluster de données est séparée de la publication, ce qui permet de persister les données avant d’exposer l’entrée.

| Élément | Contrat |
|---|---|
| Données | `fat32_write_cluster`, buffer de `sectors_per_cluster * 512` octets |
| Nom | 8.3 ASCII borné, alias explicite, pas de LFN |
| Racine | chaîne FAT32 à partir de `root_cluster` |
| Entrée | attribut, cluster haut/bas, taille 32 bits |
| Allocation dynamique | aucune |
| Tests noyau | 35/35 |
| Suite globale | 419/419 |

Les entrées LFN FAT32, l’extension automatique de la chaîne de répertoire et l’orchestrateur fichier avec rollback multi-clusters restent à traiter. Le lot est volontairement borné pour conserver une publication persistante après écriture réussie des données.

**Auteur :** Manus AI
