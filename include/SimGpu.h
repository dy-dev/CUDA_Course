#pragma once

#include "Particles.h"

#include <vector>

// Réserve sur la carte la place de `count` particules.
void gpuInit(int count);

// Avance les particules d'un pas de temps, calcul fait par la carte.
void gpuStep(std::vector<Particle>& particles, float dt);

// Libère la mémoire réservée sur la carte.
void gpuShutdown();
