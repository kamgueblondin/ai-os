# Audit d’intégration LLM — AI-OS

## État observé du projet

- AI-OS est un noyau x86 32 bits freestanding, compilé avec `-m32 -ffreestanding -nostdlib` et lancé dans QEMU.
- Le shell utilisateur inclut `ai`, `ai-mode`, `ai-help` et `ai-stats`, mais le moteur déclaré est `fake_ai`, un simulateur à mots-clés.
- L’audit des sources a trouvé des sorties série et les contrôleurs clavier, mais aucune pile réseau, pilote Ethernet, socket TCP/IP ou client HTTP(S) utilisable par le shell.
- Les artefacts actuels restent très légers : `build/ai_os.bin` ≈ 64 Kio et `my_initrd.tar` ≈ 120 Kio.

## Contraintes confirmées des fournisseurs

- La documentation Linux d’Ollama décrit une installation, un démarrage et un service système Linux : Ollama est donc un processus hôte, pas un moteur d’inférence exécutable directement par le noyau freestanding actuel.
- La documentation d’Ollama expose une API locale partiellement compatible OpenAI : `/v1/chat/completions`, `/v1/completions`, `/v1/models` et `/v1/responses` ; le modèle doit être téléchargé localement avant utilisation.
- Une API OpenAI requiert un accès réseau HTTPS et une clé gardée en dehors de l’image AI-OS.

## Conclusion provisoire

L’objectif « Ollama local et AI-OS sur une machine sans OS hôte » ne peut pas être rempli avec Ollama tel quel. Deux voies restent réalistes : (1) démarrer AI-OS dans une petite distribution Linux embarquée qui exécute Ollama comme service local ; (2) remplacer Ollama dans AI-OS par un moteur d’inférence bare-metal spécifiquement porté, avec un modèle quantifié intégré dans le support de démarrage. La seconde voie est substantiellement plus complexe et ne constitue plus une intégration Ollama.

## Sources

- https://docs.ollama.com/linux
- https://docs.ollama.com/api/openai-compatibility

