#pragma once

#include "raylib.h"

// Réglages modifiés par le panneau et relus par la boucle principale.
struct UiState
{
    int count = 0;                 // nombre de particules en vigueur
    bool resetRequested = false;
    bool useGpu = true;            // moteur actif : carte graphique ou processeur
    bool gpuAvailable = true;      // faux si le moteur GPU n'est pas construit

    // État interne des contrôles
    int pendingCount = 0;          // valeur en cours de réglage (curseur ou champ)
    bool countEditing = false;     // champ numérique en cours de saisie
};

// Mesures affichées par le panneau. Une durée négative signifie « pas de mesure ».
struct UiStats
{
    int fps = 0;
    int count = 0;
    const char* engine = "";       // moteur actif
    const char* otherEngine = "";  // l'autre moteur
    float stepMs = -1.0f;          // dernier pas du moteur actif
    float otherStepMs = -1.0f;     // dernière mesure de l'autre moteur
    float computeMs = -1.0f;       // carte : calcul du dernier pas
    float copyMs = -1.0f;          // carte : copie retour des positions
};

// Applique le style du panneau (à appeler après InitWindow).
void initUi();

// Dessine le panneau et met à jour l'état selon les actions de l'utilisateur.
void drawPanel(UiState& ui, const UiStats& stats);

// Change le nombre de particules depuis l'extérieur du panneau (raccourcis clavier).
void requestCount(UiState& ui, int count);

// Vrai si une saisie clavier est en cours dans le panneau.
bool isTyping(const UiState& ui);

// Vrai si le point (coordonnées écran) est sur le panneau.
bool isOverPanel(Vector2 point);
