#pragma once

// Constantes du monde simulé. Distances en pixels, temps en secondes.

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// Panneau de commande à gauche ; le monde occupe le reste de la fenêtre.
constexpr int kPanelWidth = 260;
constexpr float kWorldWidth = float(kWindowWidth - kPanelWidth);
constexpr float kWorldHeight = float(kWindowHeight);

constexpr float kRadius = 2.0f;
constexpr float kDt = 1.0f / 60.0f;
constexpr float kInitialSpeed = 60.0f;   // composante de vitesse initiale maximale, px/s
constexpr unsigned kSeed = 1234;

constexpr int kMinCount = 1000;
constexpr int kMaxCount = 200000;
constexpr int kDefaultCount = 10000;
constexpr int kPresetCounts[5] = { 1000, 10000, 50000, 100000, 200000 };

// Coefficient de restitution des murs : 1, rebond sans perte ; 0,5 au minimum.
constexpr float kMinRestitution = 0.5f;
constexpr float kDefaultRestitution = 1.0f;
// Souris : rayon d'action, intensités par défaut, durée maximale d'un clic simple.
constexpr float kMouseRadius = 150.0f;
constexpr float kDefaultExplosion = 400.0f;    // px/s au centre
constexpr float kDefaultAttraction = 1500.0f;  // px/s² au centre
constexpr float kMaxExplosion = 1500.0f;
constexpr float kMaxAttraction = 5000.0f;
constexpr float kClickMaxSeconds = 0.2f;

// Au-delà, le moteur processeur ignore les collisions : un pas durerait plusieurs secondes.
constexpr int kCpuCollisionLimit = 50000;

// Grille spatiale : cellules carrées d'au moins un diamètre, pour que deux particules
// en contact soient toujours dans la même cellule ou dans deux cellules voisines.
constexpr float kCellSize = 2.0f * kRadius;
constexpr int kGridWidth = int((kWorldWidth + kCellSize - 1.0f) / kCellSize);
constexpr int kGridHeight = int((kWorldHeight + kCellSize - 1.0f) / kCellSize);
constexpr int kCellCount = kGridWidth * kGridHeight;

// Threads par bloc pour les kernels de la simulation.
constexpr int kBlockSize = 256;

// Collisions par tuiles : un bloc par cellule, tuiles de kTileSize particules en mémoire partagée.
constexpr int kTileSize = 32;
