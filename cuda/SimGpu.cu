#include "SimGpu.h"

#include "Params.h"

#include <cuda_runtime.h>

namespace
{
    Particle* d_particles = nullptr;
    int allocatedCount = 0;
}

__global__ void moveParticlesKernel(Particle* d_particles, int n, float dt)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    d_particles[i].x += d_particles[i].vx * dt;
    d_particles[i].y += d_particles[i].vy * dt;
}

void gpuInit(int count)
{
    // ___TODO___ : réserver sur la carte un tableau de count particules et retenir sa taille
}

void gpuStep(std::vector<Particle>& particles, float dt)
{
    // ___TODO___ : copier les particules du processeur vers la carte
    // ___TODO___ : lancer moveParticlesKernel avec assez de blocs de kBlockSize threads pour couvrir toutes les particules
    // ___TODO___ : recopier les particules mises à jour de la carte vers le processeur
}

void gpuShutdown()
{
    // ___TODO___ : libérer le tableau réservé sur la carte
}
