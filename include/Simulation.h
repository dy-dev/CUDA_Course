#pragma once

#include "Params.h"
#include "Particles.h"

#include <vector>

enum class MouseMode
{
    None,
    Explode,    // impulsion répulsive, appliquée une fois
    Attract,    // attraction continue vers le curseur
    Swirl       // bonus : tourbillon continu autour du curseur
};

// Action de la souris sur les particules, en coordonnées du monde.
struct MouseForce
{
    MouseMode mode = MouseMode::None;
    float2 pos = { 0.0f, 0.0f };
    float strength = 0.0f;     // explosion : px/s ; attraction, tourbillon : px/s², au centre
    float radius = kMouseRadius;
};

// Recherche des collisions sur la carte.
enum class CollisionMode
{
    Direct,     // toutes les paires
    Grid        // voisinage 3 × 3 d'une grille spatiale
};

// Réglages d'un pas de simulation, fournis à chaque image.
struct StepParams
{
    float dt = kDt;
    bool walls = true;         // rebond sur les bords du monde
    float restitution = kDefaultRestitution;   // murs : part de la vitesse conservée au rebond
    bool collisions = true;    // collisions entre particules
    MouseForce mouse;
    CollisionMode collisionMode = CollisionMode::Direct;   // ignoré par le processeur
    bool needCellIds = false;  // rapatrier aussi la cellule de chaque particule (coloration)
};

// Durées des étapes du dernier pas, en ms (0 : étape non exécutée).
struct StepTimings
{
    float mouseMs = 0.0f;
    float gridMs = 0.0f;       // construction de la grille (carte, mode grille)
    float collideMs = 0.0f;
    float moveMs = 0.0f;
    float copyMs = 0.0f;       // copie retour des positions (carte seulement)
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

    // Décomposition du dernier pas.
    virtual const StepTimings& timings() const = 0;

    // Cellule de chaque particule, dans l'ordre de positions() ; nullptr si indisponible.
    virtual const std::vector<int>* cellIds() const
    {
        return nullptr;
    }

    // Nom court affiché dans le panneau.
    virtual const char* name() const = 0;
};
