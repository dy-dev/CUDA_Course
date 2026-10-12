#pragma once

#include "Simulation.h"

#include <cuda_runtime.h>

// Moteur carte graphique : les données restent sur la carte d'un pas à l'autre ;
// seules les positions reviennent, pour le dessin.
class SimGpu : public Simulation
{
public:
    void init(const Particles& state) override;
    void step(const StepParams& params) override;
    const std::vector<float2>& positions() const override;
    void readState(Particles& state) override;
    void shutdown() override;

    const char* name() const override
    {
        return "GPU";
    }

    // Durées du dernier pas, en ms : calcul sur la carte, copie retour des positions.
    float computeMs() const
    {
        return m_computeMs;
    }

    float copyMs() const
    {
        return m_copyMs;
    }

private:
    int m_count = 0;
    DeviceParticles m_device;
    std::vector<float2> m_positions;

    cudaEvent_t m_start = nullptr;
    cudaEvent_t m_afterCompute = nullptr;
    cudaEvent_t m_afterCopy = nullptr;
    float m_computeMs = 0.0f;
    float m_copyMs = 0.0f;
};
