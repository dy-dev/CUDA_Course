#include "SimGpu.h"

#include "CudaCheck.h"
#include "Params.h"

#include <cuda_runtime.h>

namespace
{
    // Démonstration : blocs de 2048 threads, au-delà de la limite de la carte (1024).
    // Le lancement est refusé ; seule la vérification d'erreurs le signale.
    constexpr bool kDemoBadLaunch = false;

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
    CUDA_CHECK(cudaMalloc(&d_particles, count * sizeof(Particle)));
    allocatedCount = count;
}

void gpuStep(std::vector<Particle>& particles, float dt)
{
    const int n = allocatedCount;
    const size_t bytes = n * sizeof(Particle);

    // Aller : tout le tableau, à chaque image.
    CUDA_CHECK(cudaMemcpy(d_particles, particles.data(), bytes, cudaMemcpyHostToDevice));

    const int blockSize = kDemoBadLaunch ? 2048 : kBlockSize;
    const int gridSize = (n + blockSize - 1) / blockSize;
    moveParticlesKernel<<<gridSize, blockSize>>>(d_particles, n, dt);
    CUDA_CHECK_LAUNCH();

    // Retour : tout le tableau, pour le dessin.
    CUDA_CHECK(cudaMemcpy(particles.data(), d_particles, bytes, cudaMemcpyDeviceToHost));
}

void gpuShutdown()
{
    CUDA_CHECK(cudaFree(d_particles));
    d_particles = nullptr;
    allocatedCount = 0;
}
