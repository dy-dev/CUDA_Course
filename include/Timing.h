#pragma once

#include <chrono>

#if defined(CUDA_PROJECT_GPU)
#include "CudaCheck.h"

#include <cuda_runtime.h>
#endif

// Chronomètre processeur : horloge monotone.
class CpuTimer
{
public:
    void start()
    {
        m_start = std::chrono::steady_clock::now();
    }

    float elapsedMs() const
    {
        return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - m_start).count();
    }

private:
    std::chrono::steady_clock::time_point m_start = std::chrono::steady_clock::now();
};

#if defined(CUDA_PROJECT_GPU)
// Chronomètre carte : deux événements placés dans la file des commandes de la
// carte. C'est la carte qui note l'instant où elle les atteint ; la durée lue
// est donc celle du travail exécuté entre les deux, pas celle des lancements.
class GpuTimer
{
public:
    GpuTimer()
    {
        CUDA_CHECK(cudaEventCreate(&m_start));
        CUDA_CHECK(cudaEventCreate(&m_stop));
    }

    ~GpuTimer()
    {
        cudaEventDestroy(m_start);
        cudaEventDestroy(m_stop);
    }

    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    void start()
    {
        CUDA_CHECK(cudaEventRecord(m_start));
    }

    void stop()
    {
        CUDA_CHECK(cudaEventRecord(m_stop));
        m_pending = true;
    }

    // Durée entre start et stop, en ms ; 0 si rien n'a été mesuré depuis la dernière lecture.
    float elapsedMs()
    {
        if (!m_pending)
        {
            return 0.0f;
        }
        m_pending = false;

        float ms = 0.0f;
        CUDA_CHECK(cudaEventSynchronize(m_stop));
        CUDA_CHECK(cudaEventElapsedTime(&ms, m_start, m_stop));
        return ms;
    }

private:
    cudaEvent_t m_start = nullptr;
    cudaEvent_t m_stop = nullptr;
    bool m_pending = false;
};
#endif

// Facteur de vitesse : combien de fois la durée `slowMs` contient `fastMs`.
inline float speedup(float slowMs, float fastMs)
{
    return fastMs > 0.0f ? slowMs / fastMs : 0.0f;
}
