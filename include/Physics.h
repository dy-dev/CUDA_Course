#pragma once

#include "Params.h"
#include "Particles.h"

// Calcul d'une particule, commun aux deux moteurs : le même code est compilé
// pour le processeur (SimCpu.cpp) et pour la carte (kernels des fichiers .cu).
// Hors de nvcc, les qualificatifs __host__ __device__ n'existent pas.
#if defined(__CUDACC__)
#define HOST_DEVICE __host__ __device__
#else
#define HOST_DEVICE
#endif

// Déplacement d'un pas de temps : position += vitesse × dt.
HOST_DEVICE inline void advance(float2& p, float2 v, float dt)
{
    p.x += v.x * dt;
    p.y += v.y * dt;
}
