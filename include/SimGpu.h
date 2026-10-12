#pragma once

#include "Simulation.h"
#include "Timing.h"

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

    const StepTimings& timings() const override
    {
        return m_timings;
    }

    const char* name() const override
    {
        return "GPU";
    }

private:
    int m_count = 0;
    DeviceParticles m_device;      // état courant
    DeviceParticles m_deviceOut;   // sortie des collisions (double tampon)
    int* d_partner = nullptr;      // partenaire d'échange de vitesse de chaque particule
    std::vector<float2> m_positions;

    // Un chronomètre par étape du pas.
    GpuTimer m_mouseTimer;
    GpuTimer m_collideTimer;
    GpuTimer m_moveTimer;
    GpuTimer m_copyTimer;
    StepTimings m_timings;
};