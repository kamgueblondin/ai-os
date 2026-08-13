# Architecture LLM bare-metal - AI-OS

## Objet

AI-OS dispose désormais d'un **plan de contrôle** pour choisir un fournisseur d'IA et un profil de modèle. La configuration de référence cible un PC x86_64 compatible UEFI, exécutant l'inférence sur CPU avec 16 Gio de mémoire. Le premier profil local déclaré est `qwen2.5-1.5b-instruct-q4_0.gguf`, une cible conversationnelle compacte au format GGUF quantifié Q4.

Le terme "bare-metal" signifie ici que le système AI-OS doit, à terme, démarrer directement depuis son support de démarrage et effectuer l'inférence sans service Linux, sans Ollama et sans processus hôte. **Ollama n'est pas intégré au noyau** : il s'agit d'un service Linux et ne peut donc pas être lancé tel quel par le noyau freestanding actuel. L'API locale compatible OpenAI d'Ollama demeure une référence d'interopérabilité, non une dépendance du chemin bare-metal [1] [2].

## Interface du shell

| Commande | Effet actuel |
|---|---|
| `ai-provider` | Affiche le fournisseur sélectionné. |
| `ai-provider local` | Sélectionne le chemin de modèle local. |
| `ai-provider openai` | Sélectionne le fournisseur en ligne et affiche clairement que le réseau et TLS restent à intégrer. |
| `ai-model list` | Affiche les modèles décrits dans le manifeste. |
| `ai-model use qwen2.5-1.5b-instruct-q4_0.gguf` | Active le profil local de référence. |
| `ai-runtime` | Affiche l'état des prérequis du moteur local et du fournisseur en ligne. |

Ces commandes constituent un contrat utilisateur stable. Pendant la phase de portage, la commande `ai <question>` conserve le binaire d'assistance existant comme **fallback de compatibilité** et annonce explicitement que le moteur GGUF n'est pas encore embarqué. Elle ne simule pas une exécution du modèle Qwen.

## Support de démarrage et manifeste

La construction de l'initrd crée `/models/models.manifest` avec le format GGUF et le profil local de référence. Le fichier de poids GGUF n'est pas inclus, car il doit être obtenu depuis une source autorisée par son détenteur puis placé dans le répertoire `/models` du support de démarrage. Cette séparation évite d'ajouter un binaire de modèle massif ou potentiellement sous licence restrictive au dépôt source.

| Élément | État | Rôle |
|---|---|---|
| Manifeste de modèles | Intégré | Déclare le format, le modèle par défaut et la famille du modèle. |
| Chargeur de blocs et système de fichiers persistant | À développer | Lit de manière fiable un GGUF depuis USB/SSD plutôt que depuis le petit initrd RAM. |
| Analyseur GGUF et tokenizer Qwen | À développer | Charge les métadonnées et transforme la question en jetons. |
| Noyau tensoriel CPU et échantillonneur | À développer | Exécute les opérations de transformeur quantifié et génère les jetons. |
| Allocation mémoire de grande capacité | À développer | Réserve les poids et le cache KV avec une gestion des pages adaptée à 16 Gio. |
| Pilote Ethernet, TCP/IP, DNS et TLS | À développer | Permet le fournisseur OpenAI sans transférer de secret dans l'image. |

## Sécurité des fournisseurs

Le fournisseur `local` n'exige aucun secret. Le fournisseur `openai` doit conserver sa clé **hors de l'image bootable et hors du dépôt Git**. Quand la pile réseau existera, l'implémentation devra préférer une saisie temporaire dans une zone mémoire effacée à la fin de session ou une configuration chiffrée par machine. Elle ne doit ni écrire la clé dans l'initrd, ni dans les journaux série, ni dans le manifeste de modèles.

## Étapes de réalisation restantes

La prochaine étape technique est de cibler un sous-ensemble minimal du format GGUF et une seule famille de tokenizers Qwen, puis de porter les primitives mémoire et calcul nécessaires. La prise en charge de "n'importe quel modèle" doit rester pilotée par des profils : chaque profil déclare une architecture, un tokenizer, une quantification et des limites de contexte, qui doivent tous être reconnus par le moteur. La compatibilité universelle n'est pas réaliste dans un premier noyau bare-metal.

## Références

[1] [Ollama - Installation et exécution sous Linux](https://docs.ollama.com/linux)

[2] [Ollama - Compatibilité avec l'API OpenAI](https://docs.ollama.com/api/openai-compatibility)

[3] [llama.cpp - moteur d'inférence LLM C/C++](https://github.com/ggml-org/llama.cpp)
