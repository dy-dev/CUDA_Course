#pragma once

#include "Particles.h"

// Grille spatiale uniforme. Les particules sont rangées par cellule : chacune
// ne cherche alors ses voisines que dans les 3 × 3 cellules qui l'entourent.
class SpatialGrid
{
public:
    // Réserve les tableaux pour `count` particules.
    void init(int count);
    void shutdown();

    // Range les particules par cellule : numéro de cellule de chacune, tri par
    // cellule, déménagement de tous les attributs dans l'ordre trié, puis plage
    // [début, fin) de chaque cellule. `particles` et `scratch` sont échangés :
    // en sortie, `particles` contient l'état trié.
    void build(DeviceParticles& particles, DeviceParticles& scratch, int count);

    // Collisions limitées au voisinage 3 × 3 ; mêmes sorties que la version
    // toutes paires : position séparée et partenaire d'échange de vitesse.
    void collide(const DeviceParticles& particles, float2* d_posOut, int* d_partner, int count);

    // Même résultat, un bloc par cellule : les particules des cellules voisines sont
    // chargées par tuiles en mémoire partagée, puis comparées à celles de la cellule.
    void collideTiled(const DeviceParticles& particles, float2* d_posOut, int* d_partner);

    // Cellule de chaque particule, dans l'ordre trié (valide après build).
    const int* cellIds() const
    {
        return d_cellId;
    }

private:
    int* d_cellId = nullptr;      // cellule de chaque particule
    int* d_order = nullptr;       // indice d'origine de chaque particule, suit le tri
    int* d_cellStart = nullptr;   // première particule de chaque cellule (-1 : vide)
    int* d_cellEnd = nullptr;     // une après la dernière
};
