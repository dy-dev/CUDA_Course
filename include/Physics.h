#pragma once

#include "Params.h"
#include "Particles.h"

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
HOST_DEVICE inline void bounce(float2& p, float2& v, float restitution)
{
    // ___TODO___ : pour chacun des quatre murs que p dépasse (à kRadius près), ramener p contre ce mur, puis inverser la composante de v perpendiculaire au mur et la multiplier par restitution
}

// Contact entre deux particules : vrai si leurs disques se chevauchent (centres
// distincts, à moins de 2 × kRadius). Donne alors la normale de contact
// unitaire, de a vers b, et la distance entre les centres.
HOST_DEVICE inline bool touching(float2 a, float2 b, float2& normal, float& dist)
{
    // ___TODO___ : renvoyer false si a et b ne se chevauchent pas ; sinon remplir dist et normal, puis renvoyer true
    return false;
}
