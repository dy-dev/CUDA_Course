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

// Action de la souris sur une particule (position p, vitesse v). Explosion :
// impulsion vers l'extérieur, appliquée une fois. Attraction : accélération
// vers le curseur. Tourbillon : l'accélération de l'attraction, tournée d'un
// quart de tour. Intensité mouse.strength au curseur, nulle à mouse.radius.
// Sur la carte, mouse.mode est un paramètre du lancement, le même pour tous
// les threads : les threads actifs d'une warp prennent tous la même branche.
HOST_DEVICE inline void applyMouseForce(float2 p, float2& v, const MouseForce& mouse, float dt)
{
    const float dx = p.x - mouse.pos.x;
    const float dy = p.y - mouse.pos.y;
    const float dist2 = dx * dx + dy * dy;
    if (dist2 >= mouse.radius * mouse.radius || dist2 == 0.0f)
    {
        return;
    }

    const float amount = mouse.mode == MouseMode::Explode ? mouse.strength : -mouse.strength * dt;
    const float dist = sqrtf(dist2);
    const float falloff = 1.0f - dist / mouse.radius;
    const float k = amount * falloff / dist;
    if (mouse.mode == MouseMode::Swirl)
    {
        v.x -= dy * k;
        v.y += dx * k;
    }
    else
    {
        v.x += dx * k;
        v.y += dy * k;
    }
}

// Collisions sur la carte : contribution de la voisine j (position pj, vitesse
// vj) à la particule (p, v). Le recul est cumulé dans dp ; le partenaire
// d'échange de vitesse est la plus proche des voisines qui se rapprochent
// (partnerDist : distance du partenaire retenu, 2 × kRadius au départ).
HOST_DEVICE inline void accumulateContact(float2 p, float2 v, float2 pj, float2 vj, int j,
                                          float2& dp, int& partner, float& partnerDist)
{
    float2 normal;
    float dist;
    if (!touching(p, pj, normal, dist))
    {
        return;
    }

    const float half = 0.5f * (2.0f * kRadius - dist);
    dp.x -= normal.x * half;
    dp.y -= normal.y * half;

    const float vn = (vj.x - v.x) * normal.x + (vj.y - v.y) * normal.y;
    if (vn < 0.0f && dist < partnerDist)
    {
        partner = j;
        partnerDist = dist;
    }
}
