#pragma once

#include "Params.h"
#include "Particles.h"
#include "Simulation.h"   // MouseForce

#include <math.h>

// Calcul d'une particule, commun aux deux moteurs : le même code est compilé
// pour le processeur (SimCpu.cpp) et pour la carte (kernels des fichiers .cu).
// Hors de nvcc, les qualificatifs __host__ __device__ n'existent pas.
#if defined(__CUDACC__)
#define HOST_DEVICE __host__ __device__
#else
#define HOST_DEVICE
#endif

// Déplacement d'un pas de temps : position += vitesse × dt.
HOST_DEVICE inline void advance(float2& p, float2 v, float dt)
{
    p.x += v.x * dt;
    p.y += v.y * dt;
}

// Rebond sur les murs du monde. Une particule qui dépasse un mur (à kRadius
// près) est ramenée contre lui ; sa composante de vitesse perpendiculaire au
// mur est inversée et multipliée par restitution (1 : rebond sans perte).
// Le signe est imposé (vers l'intérieur du monde) plutôt que simplement
// changé : une particule déjà repartie ne repasse pas derrière le mur.
HOST_DEVICE inline void bounce(float2& p, float2& v, float restitution)
{
    if (p.x < kRadius)
    {
        p.x = kRadius;
        v.x = restitution * fabsf(v.x);
    }
    else if (p.x > kWorldWidth - kRadius)
    {
        p.x = kWorldWidth - kRadius;
        v.x = -restitution * fabsf(v.x);
    }
    if (p.y < kRadius)
    {
        p.y = kRadius;
        v.y = restitution * fabsf(v.y);
    }
    else if (p.y > kWorldHeight - kRadius)
    {
        p.y = kWorldHeight - kRadius;
        v.y = -restitution * fabsf(v.y);
    }
}

// Contact entre deux particules : vrai si leurs disques se chevauchent (centres
// distincts, à moins de 2 × kRadius). Donne alors la normale de contact
// unitaire, de a vers b, et la distance entre les centres.
HOST_DEVICE inline bool touching(float2 a, float2 b, float2& normal, float& dist)
{
    const float minDist = 2.0f * kRadius;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 >= minDist * minDist || dist2 == 0.0f)
    {
        return false;
    }

    dist = sqrtf(dist2);
    normal.x = dx / dist;
    normal.y = dy / dist;
    return true;
}

// Action de la souris sur une particule (position p, vitesse v) : explosion,
// appliquée une fois, ou attraction, continue. Intensité mouse.strength au
// curseur, nulle à mouse.radius.
HOST_DEVICE inline void applyMouseForce(float2 p, float2& v, const MouseForce& mouse, float dt)
{
    // ___TODO___ : si p est à moins de mouse.radius du curseur, ajouter à v une contribution d'intensité mouse.strength au centre, décroissant jusqu'à zéro au bord ; dirigée à l'opposé du curseur pour une explosion (impulsion), vers le curseur pour une attraction (accélération, multipliée par dt)
}
