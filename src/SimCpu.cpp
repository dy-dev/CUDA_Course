#include "SimCpu.h"

#include "Physics.h"

void SimCpu::init(const Particles& state)
{
    m_particles = state;
}

void SimCpu::step(const StepParams& params)
{
    // ___TODO___ : avancer chaque particule de m_particles d'un pas de temps params.dt, avec la fonction partagée advance (include/Physics.h)
}

const std::vector<float2>& SimCpu::positions() const
{
    return m_particles.pos;
}

void SimCpu::readState(Particles& state)
{
    // ___TODO___ : recopier dans state l'état complet du moteur : positions et vitesses
}

void SimCpu::shutdown()
{
    m_particles = Particles();
}
