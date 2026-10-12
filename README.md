# Particules

Simulation 2D de particules, calculée sur le processeur ou sur la carte graphique (CUDA), affichée avec raylib et pilotée par un panneau raygui.

Ce dépôt se complète au fil du cours ; chaque étape arrive sous forme d'un tag.

## Prérequis

- CMake 3.24 ou plus récent
- Pour le moteur GPU : toolkit CUDA (testé avec la version 13.4) et une carte NVIDIA d'architecture 8.6 ou 8.9 ; sans eux, voir « Sans carte NVIDIA »
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

Si la configuration affiche « Moteur GPU : OFF » sur une machine équipée d'une carte NVIDIA et du toolkit CUDA, c'est en général qu'une chaîne MinGW est restée sélectionnée : nvcc n'a pas été retenu, et seul le moteur CPU est construit.

## Sans carte NVIDIA (Mac, portable Intel ou AMD)

Le moteur GPU demande une carte NVIDIA et le toolkit CUDA. Sans eux, le projet se construit quand même, avec le seul moteur CPU :

- à la configuration, CMake cherche un compilateur CUDA ; s'il n'en trouve pas, il affiche `Moteur GPU : OFF` et ne compile aucun fichier `.cu` ;
- on peut aussi écarter le moteur GPU volontairement : `cmake -S . -B build -DCUDA_PROJECT_GPU=OFF` ;
- dans l'application, la bascule CPU / GPU est grisée, le titre de la section indique « Moteur : GPU absent » et la touche `G` est sans effet ;
- `bench_gpu` n'est pas construit.

Le moteur CPU et les fonctions de calcul qu'il partage avec la carte (`include/Physics.h`) se compilent et s'exécutent normalement. Les kernels se lisent et s'écrivent de la même façon, mais ne s'exécutent que sur une machine équipée d'une carte NVIDIA.

Sur Mac, il suffit de CMake et des outils en ligne de commande de Xcode (`xcode-select --install`). Cette configuration n'a été vérifiée que sous Windows (avec `-DCUDA_PROJECT_GPU=OFF`) ; elle n'a pas encore été essayée sur Mac.

## Programmes

| Programme   | Rôle |
|-------------|------|
| `particles` | Application : fenêtre, panneau de commande, simulation |
| `bench_cpu` | Mesure console du déplacement sur le processeur, écrit `cpu_ms.csv` |
| `bench_gpu` | Même mesure sur la carte graphique, comparée à `cpu_ms.csv` ; puis lecture de x seul en AoS et en SoA |

Lancer `bench_cpu` puis `bench_gpu` depuis le même dossier : le second relit le fichier écrit par le premier.

## Commandes de l'application

| Commande | Effet |
|----------|-------|
| Curseur ou champ « Particules » | Nombre de particules (1 000 à 200 000), appliqué au relâchement ou en fin de saisie |
| Touches `1` à `5` | 1 000 / 10 000 / 50 000 / 100 000 / 200 000 particules |
| Bouton « Réinitialiser » | Repart de l'état initial (graine fixe) |
| Bascule CPU / GPU, touche `G` | Change de moteur de calcul en cours de simulation (l'état des particules est transmis) |
| Cases « Murs », « Collisions » | Rebond sur les bords, collisions entre particules |
| Curseur « Restitution » | Part de la vitesse conservée au rebond sur un mur, de 0,5 à 1 (1 : rebond sans perte) |
| `Échap` | Quitter |

Le moteur CPU calcule les collisions entre toutes les paires : au-delà de 50 000 particules, un pas durerait plusieurs secondes ; il les ignore donc (le panneau le signale).

## Organisation

```
CMakeLists.txt
bench/      mesures console
include/    en-têtes
  Params.h      constantes du monde (taille, rayon, pas de temps…)
  Particles.h   données des particules (structure de tableaux)
  Physics.h     calcul d'une particule, commun aux deux moteurs (__host__ __device__) :
                déplacement, rebond sur les murs, contact entre deux particules
  Simulation.h  interface commune aux deux moteurs
  SimCpu.h      moteur processeur
  SimGpu.h      moteur carte graphique
  CudaCheck.h   vérification des appels CUDA
  Ui.h          panneau raygui
src/        application (C++)
  main.cpp      fenêtre, boucle, entrées
  SimCpu.cpp    moteur processeur
  Ui.cpp        panneau raygui
cuda/       code CUDA (nvcc)
  SimGpu.cu     moteur carte graphique
```
