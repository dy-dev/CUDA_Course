#pragma once

#include "Simulation.h"

// Moteur processeur : boucles séquentielles sur les particules.
class SimCpu : public Simulation
{
public:
    void init(const Particles& state) override;
    void step(const StepParams& params) override;
    const std::vector<float2>& positions() const override;
    void readState(Particles& state) override;
    void shutdown() override;

    const char* name() const override
    {
        return "CPU";
    }

private:
    Particles m_particles;
};
