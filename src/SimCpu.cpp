#include "SimCpu.h"

#include "Physics.h"
#include "Timing.h"

namespace
{
    // Collisions entre toutes les paires de particules (masses égales).
    void collideAllPairs(Particles& particles)
    {
        const float minDist = 2.0f * kRadius;

        const int n = particles.count();
        for (int i = 0; i < n; ++i)
        {
            for (int j = i + 1; j < n; ++j)
            {
                float2& pi = particles.pos[i];
                float2& pj = particles.pos[j];
                float2 normal;
                float dist;
                if (!touching(pi, pj, normal, dist))
                {
                    continue;
                }

                // Séparation : chacune recule de la moitié du chevauchement.
                const float half = 0.5f * (minDist - dist);
                pi.x -= normal.x * half;
                pi.y -= normal.y * half;
                pj.x += normal.x * half;
                pj.y += normal.y * half;

                // Si elles se rapprochent, elles échangent leurs composantes normales.
                float2& vi = particles.vel[i];
                float2& vj = particles.vel[j];
                const float vn = (vj.x - vi.x) * normal.x + (vj.y - vi.y) * normal.y;
                if (vn < 0.0f)
                {
                    vi.x += vn * normal.x;
                    vi.y += vn * normal.y;
                    vj.x -= vn * normal.x;
                    vj.y -= vn * normal.y;
                }
            }
        }
    }
}

void SimCpu::init(const Particles& state)
{
    m_particles = state;
}

void SimCpu::step(const StepParams& params)
{
    m_timings = StepTimings();
    CpuTimer timer;

    const int n = m_particles.count();
    if (params.mouse.mode != MouseMode::None)
    {
        timer.start();
        for (int i = 0; i < n; ++i)
        {
            applyMouseForce(m_particles.pos[i], m_particles.vel[i], params.mouse, params.dt);
        }
        m_timings.mouseMs = timer.elapsedMs();
    }
    if (params.collisions && n <= kCpuCollisionLimit)
    {
        timer.start();
        collideAllPairs(m_particles);
        m_timings.collideMs = timer.elapsedMs();
    }

    // Déplacement, puis rebond éventuel sur les murs : fonctions partagées avec la carte.
    timer.start();
    for (int i = 0; i < n; ++i)
    {
        advance(m_particles.pos[i], m_particles.vel[i], params.dt);
        if (params.walls)
        {
            bounce(m_particles.pos[i], m_particles.vel[i], params.restitution);
        }
    }
    m_timings.moveMs = timer.elapsedMs();
}

float SimCpu::meanKineticEnergy()
{
    const int n = m_particles.count();
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float2 v = m_particles.vel[i];
        sum += 0.5f * (v.x * v.x + v.y * v.y);
    }
    return n > 0 ? sum / float(n) : 0.0f;
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
