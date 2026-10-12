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
