# Particules

Simulation 2D de particules, calculée sur le processeur ou sur la carte graphique (CUDA), affichée avec raylib et pilotée par un panneau raygui.

Ce dépôt se complète au fil du cours ; chaque étape arrive sous forme d'un tag.

## Prérequis

- CMake 3.24 ou plus récent
- Toolkit CUDA (testé avec la version 13.4) et une carte NVIDIA d'architecture 8.6 ou 8.9
- Windows : Visual Studio 2022 avec l'intégration CUDA
- Linux : GCC ou Clang, et les paquets de développement X11/OpenGL demandés par raylib
  (`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`)

raylib 5.5 et raygui 4.0 sont téléchargés automatiquement à la première configuration (connexion requise).

## Compilation

Windows (Visual Studio, Debug et Release dans le même dossier) :

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cmake --build build --config Debug
```

Linux :

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Les mesures de performance se font **toujours en Release**. La configuration Debug sert à la mise au point.

La compilation sous Ubuntu 22.04 / 24.04 n'a pas encore été vérifiée : elle le sera sur les machines distantes.

## Sous Windows

nvcc ne compile pas seul le code hôte : il s'appuie sur le compilateur de Visual Studio (`cl.exe`). Il faut donc la chaîne d'outils Visual Studio 2022 (charge de travail « Développement Desktop en C++ »), installée avant le toolkit CUDA pour que celui-ci s'y intègre. MinGW ne convient pas : nvcc ne l'accepte pas sous Windows.

Dans CLion :

1. *Settings → Build, Execution, Deployment → Toolchains* : ajouter une chaîne **Visual Studio**, architecture `amd64`, et la placer en tête de liste.
2. *Settings → Build, Execution, Deployment → CMake* : dans les profils Debug et Release, choisir cette chaîne (champ *Toolchain*).
3. Recharger le projet (*File → Reload CMake Project*).

Une configuration qui échoue sur « No CMAKE_CUDA_COMPILER could be found » ou sur `cl.exe` vient en général d'une chaîne MinGW restée sélectionnée.

## Programmes

| Programme   | Rôle |
|-------------|------|
| `particles` | Application : fenêtre, panneau de commande, simulation |
| `bench_cpu` | Mesure console du déplacement sur le processeur, écrit `cpu_ms.csv` |
| `bench_gpu` | Même mesure sur la carte graphique, comparée à `cpu_ms.csv` |

Lancer `bench_cpu` puis `bench_gpu` depuis le même dossier : le second relit le fichier écrit par le premier.

## Commandes de l'application

| Commande | Effet |
|----------|-------|
| Curseur ou champ « Particules » | Nombre de particules (1 000 à 200 000), appliqué au relâchement ou en fin de saisie |
| Touches `1` à `5` | 1 000 / 10 000 / 50 000 / 100 000 / 200 000 particules |
| Bouton « Réinitialiser » | Repart de l'état initial (graine fixe) |
| `Échap` | Quitter |

## Organisation

```
CMakeLists.txt
bench/      mesures console
include/    en-têtes
  Params.h     constantes du monde (taille, rayon, pas de temps…)
  Particles.h  données des particules
  SimGpu.h     calcul sur la carte graphique
  Ui.h         panneau raygui
src/        application (C++)
  main.cpp     fenêtre, boucle, entrées
  Ui.cpp       panneau raygui
cuda/       code CUDA (nvcc)
  SimGpu.cu    calcul sur la carte graphique
```
