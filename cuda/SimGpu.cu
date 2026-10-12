#include "SimGpu.h"

#include "CudaCheck.h"
#include "Params.h"
#include "Physics.h"

namespace
{
    // Démonstration : blocs de 2048 threads, au-delà de la limite de la carte (1024).
    // Le lancement est refusé ; seule la vérification d'erreurs le signale.
    constexpr bool kDemoBadLaunch = false;

    // Nombre de blocs pour couvrir n éléments (arrondi supérieur).
    int gridFor(int n, int blockSize)
    {
        return (n + blockSize - 1) / blockSize;
    }
}

// Déplacement d'un pas de temps, par la fonction partagée avec le moteur CPU.
__global__ void moveParticlesKernel(float2* d_pos, const float2* d_vel, int n, float dt)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    float2 p = d_pos[i];
    advance(p, d_vel[i], dt);
    d_pos[i] = p;
}

void SimGpu::init(const Particles& state)
{
    m_count = state.count();
    const size_t bytes = m_count * sizeof(float2);

    // Seule copie aller : les données restent ensuite sur la carte.
    CUDA_CHECK(cudaMalloc(&m_device.d_pos, bytes));
    CUDA_CHECK(cudaMalloc(&m_device.d_vel, bytes));
    CUDA_CHECK(cudaMemcpy(m_device.d_pos, state.pos.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m_device.d_vel, state.vel.data(), bytes, cudaMemcpyHostToDevice));
    m_positions = state.pos;

    CUDA_CHECK(cudaEventCreate(&m_start));
    CUDA_CHECK(cudaEventCreate(&m_afterCompute));
    CUDA_CHECK(cudaEventCreate(&m_afterCopy));
}

void SimGpu::step(const StepParams& params)
{
    CUDA_CHECK(cudaEventRecord(m_start));

    const int blockSize = kDemoBadLaunch ? 2048 : kBlockSize;
    moveParticlesKernel<<<gridFor(m_count, blockSize), blockSize>>>(m_device.d_pos, m_device.d_vel, m_count, params.dt);
    CUDA_CHECK_LAUNCH();
    CUDA_CHECK(cudaEventRecord(m_afterCompute));

    // Copie retour des seules positions, pour le dessin.
    CUDA_CHECK(cudaMemcpy(m_positions.data(), m_device.d_pos, m_count * sizeof(float2), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(m_afterCopy));

    CUDA_CHECK(cudaEventSynchronize(m_afterCopy));
    CUDA_CHECK(cudaEventElapsedTime(&m_computeMs, m_start, m_afterCompute));
    CUDA_CHECK(cudaEventElapsedTime(&m_copyMs, m_afterCompute, m_afterCopy));
}

const std::vector<float2>& SimGpu::positions() const
{
    return m_positions;
}

void SimGpu::readState(Particles& state)
{
    // ___TODO___ : dimensionner state puis y recopier les positions et les vitesses depuis la carte
}

void SimGpu::shutdown()
{
    if (m_count == 0)
    {
        return;
    }

    CUDA_CHECK(cudaFree(m_device.d_pos));
    CUDA_CHECK(cudaFree(m_device.d_vel));
    m_device = DeviceParticles();
    m_positions.clear();

    CUDA_CHECK(cudaEventDestroy(m_start));
    CUDA_CHECK(cudaEventDestroy(m_afterCompute));
    CUDA_CHECK(cudaEventDestroy(m_afterCopy));
    m_count = 0;
}
