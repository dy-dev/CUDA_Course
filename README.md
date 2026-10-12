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
| « Collisions sur la carte » : Directes / Grille | Toutes les paires, ou voisinage 3 × 3 d'une grille spatiale (moteur GPU) |
| Case « Colorer par cellule », touche `C` | Couleur selon la cellule de la grille (moteur GPU, mode grille) |
| Touche `V` | Recalcule sur le processeur l'énergie cinétique moyenne affichée, pour contrôle |
| Clic bref dans le monde | Explosion : impulsion répulsive autour du curseur |
| Bouton maintenu dans le monde | Attraction continue vers le curseur |
| Clic droit maintenu dans le monde | Bonus : tourbillon autour du curseur (voir « Bonus : tourbillon ») |
| Curseurs « Explosion », « Attraction » | Intensité des effets ; « Attraction » règle aussi le tourbillon |
| Bouton « Exporter CSV », touche `M` | Écrit la durée des 120 dernières images, étape par étape, dans `frame_times.csv` |
| `Échap` | Quitter |

Sur la carte, chaque particule choisit pour l'échange de vitesse sa voisine la plus proche parmi celles qui s'en rapprochent ; l'échange n'a lieu qu'entre partenaires mutuels. Les échanges se font ainsi par paires disjointes : l'énergie et la quantité de mouvement sont conservées, comme dans la boucle séquentielle du processeur.

La grille spatiale n'existe que sur la carte : le moteur CPU garde volontairement les collisions entre toutes les paires (O(N²)), pour garder le contraste entre les deux approches. Il calcule les collisions entre toutes les paires : au-delà de 50 000 particules, un pas durerait plusieurs secondes ; il les ignore donc (le panneau le signale).

## Mesurer et profiler

Le panneau décompose la dernière image : étapes du pas (souris, collisions, déplacement, copie retour), préparation du dessin, et le reste (affichage, attente de la cadence de 60 images/s). Sur la carte, chaque étape est chronométrée par une paire d'événements CUDA ; sur le processeur, par l'horloge monotone. Le bouton « Exporter CSV » (ou `M`) écrit les 120 dernières images dans `frame_times.csv`.

Pour aller plus loin sur une machine distante, sans interface graphique de profilage :

```
# Chronologie complète (kernels, copies, appels API), rapport ouvert ensuite en local avec Nsight Systems
nsys profile -o particules --trace=cuda,osrt ./build/particles

# Détail d'un kernel (occupation, accès mémoire), rapport ouvert en local avec Nsight Compute
ncu --kernel-name collideAllPairsKernel --launch-count 5 -o collisions ./build/particles
```

Rapatrier ensuite `particules.nsys-rep` et `collisions.ncu-rep` (par `scp`) et les ouvrir dans Nsight Systems / Nsight Compute sur le poste local. Les kernels sont compilés avec `-lineinfo` : Nsight Compute relie les mesures aux lignes du source.

## Bonus : tourbillon

Clic droit maintenu dans le monde : les particules proches du curseur se mettent à tourner autour de lui (cercle bleu). La force reprend le rayon d'action et l'intensité de l'attraction, décroissance comprise, mais sa direction est perpendiculaire au vecteur particule → curseur au lieu de le suivre : c'est l'accélération de l'attraction tournée d'un quart de tour.

Le mode est le même pour toute la grille : il arrive dans les paramètres du lancement (`MouseForce`), et non d'un calcul propre à chaque thread. La branche qui sépare tourbillon et attraction dépend donc d'une valeur identique dans toute la warp : tous les threads actifs prennent le même chemin, sans divergence. La seule branche divergente du kernel reste celle qui existait déjà, le test de distance au curseur.

En pratique, les particules ne tournent pas en rond : elles spiralent en s'éloignant du curseur. À chaque pas, la vitesse calculée au début du pas est appliquée telle quelle pendant tout le pas : la particule avance en ligne droite le long de la tangente, et une tangente s'écarte toujours du cercle. L'intégration explicite ajoute ainsi un peu de distance à chaque pas, et cette dérive se cumule. La force elle-même n'a d'ailleurs aucune composante dirigée vers le curseur pour retenir les particules sur une orbite.

Obtenir un tourbillon stable reste une question ouverte.

## Organisation

```
CMakeLists.txt
bench/      mesures console
include/    en-têtes
  Params.h        constantes du monde (taille, rayon, pas de temps…)
  Particles.h     données des particules (structure de tableaux)
  Physics.h       calcul d'une particule, commun aux deux moteurs (__host__ __device__) :
                  déplacement, rebond sur les murs, contact entre deux particules,
                  action de la souris
  Simulation.h    interface commune aux deux moteurs
  SimCpu.h        moteur processeur
  SimGpu.h        moteur carte graphique
  SpatialGrid.h   grille spatiale (tri par cellule, voisinage 3 × 3)
  CudaCheck.h     vérification des appels CUDA
  Timing.h        chronomètres processeur et carte (événements CUDA)
  Ui.h            panneau raygui
src/        application (C++)
  main.cpp        fenêtre, boucle, entrées
  SimCpu.cpp      moteur processeur
  Ui.cpp          panneau raygui (seul fichier qui compile l'implémentation de raygui)
cuda/       code CUDA (nvcc)
  SimGpu.cu       moteur carte graphique
  SpatialGrid.cu  grille spatiale (tri par cellule, voisinage 3 × 3)
```
