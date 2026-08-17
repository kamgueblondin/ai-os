# AOS-113 — Diagnostic réseau machine-lisible

## Objectif

Le lot AOS-113 ajoute une représentation JSON déterministe à la commande `net-status`. Cette représentation permet aux scripts de démarrage, aux tests QEMU et aux outils de diagnostic de distinguer explicitement l’absence de transport réseau d’un succès OpenAI.

## Contrat

La commande interactive historique reste disponible :

```text
net-status
```

La variante machine-lisible est :

```text
net-status json
```

Elle produit une seule ligne JSON compacte, sans texte décoratif :

```json
{"nic":"absent","ethernet":"absent","arp":"absent","ipv4":"absent","dhcp":"absent","dns":"absent","tcp":"absent","tls":"absent","openai":"blocked"}
```

| Champ | Valeur actuelle | Signification |
|---|---|---|
| `nic` | `absent` | Aucun pilote de carte réseau n’est initialisé dans le noyau. |
| `ethernet` | `absent` | Aucune trame Ethernet n’est émise ou reçue. |
| `arp` | `absent` | La résolution d’adresses matérielles n’est pas disponible. |
| `ipv4` | `absent` | Aucune configuration IPv4 n’est attribuée. |
| `dhcp` | `absent` | Aucun bail DHCP n’est demandé. |
| `dns` | `absent` | Aucune résolution de nom n’est possible. |
| `tcp` | `absent` | Aucun flux TCP n’est ouvert. |
| `tls` | `absent` | Aucun canal TLS n’est établi et aucun certificat n’est validé. |
| `openai` | `blocked` | Le fournisseur en ligne est refusé tant qu’un transport complet n’existe pas. |

> Le champ `openai=blocked` est volontaire : la sélection du profil `ai-provider openai` ne doit jamais être interprétée comme l’émission effective d’une requête réseau.

## Validation

Le test `tests/scripts/test_ai_provider_commands.py` vérifie désormais les deux formes de diagnostic. Il démarre l’image i386 sous QEMU, exécute `net-status`, puis exécute deux fois `net-status json` et recherche les champs `nic=absent` et `openai=blocked` dans la sortie série.

La suite de non-régression reste à **265 tests verts**. Le build complet doit être lancé avec `make all`, car la première cible du Makefile est un artefact assembleur implicite lorsqu’aucune cible n’est explicitement demandée. Le smoke ciblé est ensuite exécuté par :

```sh
make test-all
make all
python3 tests/scripts/test_ai_provider_commands.py
```

Ce lot ne prétend pas implémenter le réseau. Les prochaines étapes restent le pilote NIC, Ethernet/ARP, IPv4/DHCP, DNS/TCP, puis TLS et le client OpenAI borné.

## Statut

AOS-113 est une amélioration de diagnostic et de contrat de contrôle, sans allocation dynamique et sans modification du chemin critique GGUF. Il est compatible avec le mode texte historique et ne produit aucun paquet réseau.
