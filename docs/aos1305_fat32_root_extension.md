# AOS-1305 à AOS-1320 — extension automatique du répertoire racine FAT32

> **État :** implémenté et validé localement. **Suite globale : 420/420 tests verts.**

`fat32_extend_root_directory` parcourt la chaîne du répertoire racine jusqu’à son dernier cluster EOC, réserve un nouveau cluster, l’efface dans le buffer statique borné, l’écrit dans la région de données puis remplace l’EOC du dernier cluster par le nouveau lien. En cas d’échec d’écriture ou de chaînage, l’entrée du cluster nouvellement réservé est libérée.

L’API restitue le nouveau cluster à l’appelant et ne publie aucune entrée LFN. La création 8.3 peut utiliser cette primitive lorsque la recherche de slot libre arrive en fin de chaîne ; l’intégration automatique dans `fat32_create_root_entry` est conservée comme étape suivante afin de garder le comportement d’échec actuel explicitement observable.

| Propriété | Garantie |
|---|---|
| Chaîne analysée | racine FAT32 et liens validés avec garde bornée |
| Nouveau cluster | réservé EOC puis nettoyé avant usage |
| Persistance | écriture des secteurs caller-owned via writer explicite |
| Échec | nouveau cluster libéré, chaîne précédente intacte |
| LFN | non inclus dans ce sous-lot |
| Tests noyau | 36/36 |
| Suite globale | 420/420 |

Le prochain sous-lot peut intégrer cette extension dans la publication d’entrées LFN FAT32 et ajouter la génération checksum/UTF-16LE, sans modifier l’ABI publique existante.

**Auteur :** Manus AI
