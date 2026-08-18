# AOS-729 à AOS-736 — Exponentiation modulaire à exposant multi-limb

**Auteur :** Manus AI

**Statut :** implémenté et validé localement

**Périmètre :** prérequis arithmétique borné pour les inverses modulaires de P-256 et la future vérification ECDSA.

## Objectif

La pile bigint disposait d’une exponentiation modulaire limitée à un exposant `uint32_t`. Cette limite empêche de calculer les inverses de corps ou d’ordre de courbe requis par une vérification ECDSA P-256, dont les exposants ont 256 bits. AOS-729 à AOS-736 introduit `bigint_modexp`, une variante à exposant `bigint_t` qui conserve le contrat sans allocation dynamique et l’espace de travail détenu par l’appelant.

| Élément | Contrat |
|---|---|
| Base | `bigint_t` arbitraire, réduite modulo le module avant calcul. |
| Exposant | `bigint_t` de largeur bornée par son buffer appelant. |
| Module | Positif et non nul. |
| Workspace | Au moins `4 × modulus->length` limbs. |
| Mémoire | Aucun état global, heap ou allocation dynamique. |
| Algorithme | Square-and-multiply, lecture du bit le plus significatif vers le moins significatif. |

## Sûreté et intégration

L’implémentation emploie quatre segments de workspace : résultat, base réduite, produit intermédiaire et temporaire de multiplication modulaire. Les objets bigint locaux ne référencent que ces zones fournies par l’appelant. L’API rejette une sortie trop courte ou un workspace insuffisant avant tout calcul.

> Cette primitive est un prérequis de compatibilité ECDSA ; elle ne rend pas encore la pile TLS compatible ECDSA. Aucun certificat ECDSA, aucune suite `ECDHE_ECDSA`, aucune clé ou aucun endpoint supplémentaire n’est accepté par ce lot.

La boucle de calcul n’est pas annoncée constante en temps : les futurs appels devront distinguer les exposants publics de validation (par exemple les constantes d’inversion publique) des secrets. La future primitive ECDSA conservera des chemins explicitement audités pour cette distinction.

## Régressions

Le test Unity couvre un exposant sur deux limbs avec le vecteur déterministe suivant :

| Expression | Résultat attendu |
|---|---:|
| `7^(2^32 + 1) mod 101` | `48` |

Le test vérifie aussi le rejet d’un workspace inférieur aux quatre limbs requis. Les validations locales ont produit les résultats suivants.

| Vérification | Résultat |
|---|---|
| Suite unitaire complète | **385/385** tests réussis. |
| Build i386 freestanding | Réussi. |
| Smoke QEMU fournisseur | Réussi. |
| Smoke QEMU NE2000 | Réussi. |

## Limites connues

ECDSA P-256 reste à implémenter : arithmétique de courbe, validation de point, parsing ASN.1 `r,s`, clé publique X.509 `id-ecPublicKey`, signatures de certificats ECDSA, signature `ServerKeyExchange`, extension ClientHello et suite `ECDHE_ECDSA`. La révocation, les chaînes de confiance multiples, les credentials OpenAI, la fermeture/annulation, timeout/retry, outils, multimodal et Unicode complet restent également hors périmètre.

## Références

[1] [RFC 8422 — ECC Cipher Suites for TLS Versions 1.2 and Earlier](https://www.rfc-editor.org/rfc/rfc8422)

[2] [RFC 5246 — The Transport Layer Security Protocol Version 1.2](https://datatracker.ietf.org/doc/html/rfc5246)
