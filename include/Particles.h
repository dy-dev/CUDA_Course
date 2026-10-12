#pragma once

#if defined(CUDA_PROJECT_GPU) || defined(__CUDACC__)
#include <vector_types.h>   // float2
#else
// Sans CUDA : même disposition que le float2 de CUDA.
struct alignas(8) float2
{
    float x;
    float y;
};
#endif

#include <vector>

// Particules côté processeur : une structure de tableaux. L'élément i de chaque
// tableau décrit la particule i. Positions en px, vitesses en px/s.
struct Particles
{
    std::vector<float2> pos;
    std::vector<float2> vel;

    int count() const
    {
        return int(pos.size());
    }
};

// Mêmes tableaux, réservés sur la carte graphique.
struct DeviceParticles
{
    float2* d_pos = nullptr;
    float2* d_vel = nullptr;
};
