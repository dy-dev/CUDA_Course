#pragma once

#include "Simulation.h"
#include "SpatialGrid.h"
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
    float meanKineticEnergy() override;

    const StepTimings& timings() const override
    {
        return m_timings;
    }

    const std::vector<int>* cellIds() const override
    {
        return m_hasCellIds ? &m_cellIds : nullptr;
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
    float* d_blockSums = nullptr;  // réduction : un total par bloc
    float* d_total = nullptr;      // réduction : total final
    std::vector<float2> m_positions;
    SpatialGrid m_grid;
    std::vector<int> m_cellIds;
    bool m_hasCellIds = false;

    // Un chronomètre par étape du pas.
    GpuTimer m_mouseTimer;
    GpuTimer m_gridTimer;
    GpuTimer m_collideTimer;
    GpuTimer m_moveTimer;
    GpuTimer m_copyTimer;
    StepTimings m_timings;
};