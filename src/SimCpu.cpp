#include "SimCpu.h"

#include "Physics.h"

namespace
{
    // Collisions entre toutes les paires de particules (masses égales).
    void collideAllPairs(Particles& particles)
    {
        // ___TODO___ : pour chaque paire en contact (fonction partagée touching), écarter les deux particules de la moitié du chevauchement chacune le long de la normale de contact et, si elles se rapprochent, échanger leurs composantes de vitesse le long de cette normale
    }
}

void SimCpu::init(const Particles& state)
{
    m_particles = state;
}

void SimCpu::step(const StepParams& params)
{
    if (params.collisions && m_particles.count() <= kCpuCollisionLimit)
    {
        collideAllPairs(m_particles);
    }

    // Déplacement, puis rebond éventuel sur les murs : fonctions partagées avec la carte.
    const int n = m_particles.count();
    for (int i = 0; i < n; ++i)
    {
        advance(m_particles.pos[i], m_particles.vel[i], params.dt);
        if (params.walls)
        {
            bounce(m_particles.pos[i], m_particles.vel[i], params.restitution);
        }
    }
}

const std::vector<float2>& SimCpu::positions() const
{
    return m_particles.pos;
}

void SimCpu::readState(Particles& state)
{
    state = m_particles;
}

void SimCpu::shutdown()
{
    m_particles = Particles();
}
