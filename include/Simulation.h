#pragma once

#include "Params.h"
#include "Particles.h"

#include <vector>

// Réglages d'un pas de simulation, fournis à chaque image.
struct StepParams
{
    float dt = kDt;
};

// Interface commune aux moteurs de calcul : processeur et carte graphique.
class Simulation
{
public:
    virtual ~Simulation() = default;

    // Prend en charge l'état donné (positions et vitesses).
    virtual void init(const Particles& state) = 0;

    // Avance d'un pas de temps.
    virtual void step(const StepParams& params) = 0;

    // Positions à jour, pour le dessin.
    virtual const std::vector<float2>& positions() const = 0;

    // Recopie l'état complet dans `state`, pour le confier à un autre moteur.
    virtual void readState(Particles& state) = 0;

    // Libère les ressources du moteur.
    virtual void shutdown() = 0;

    // Nom court affiché dans le panneau.
    virtual const char* name() const = 0;
};
