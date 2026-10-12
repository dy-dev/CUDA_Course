#pragma once

#include "Simulation.h"

#include "raylib.h"

// Réglages modifiés par le panneau et relus par la boucle principale.
struct UiState
{
    int count = 0;                 // nombre de particules en vigueur
    bool resetRequested = false;
    bool useGpu = true;            // moteur actif : carte graphique ou processeur
    bool gpuAvailable = true;      // faux si le moteur GPU n'est pas construit
    bool walls = true;             // rebond sur les bords du monde
    float restitution = 1.0f;      // murs : part de la vitesse conservée au rebond
    bool collisions = true;        // collisions entre particules
    float explosion = 0.0f;        // intensité d'un clic bref, px/s
    float attraction = 0.0f;       // intensité du bouton maintenu, px/s²
    bool exportRequested = false;  // écrire l'historique des durées dans un fichier CSV
    bool verifyRequested = false;  // recalculer la statistique sur le processeur
    int collisionMode = 1;         // carte : 0 toutes les paires, 1 grille spatiale
    bool colorByCell = false;      // couleur selon la cellule de la grille

    // État interne des contrôles
    int pendingCount = 0;          // valeur en cours de réglage (curseur ou champ)
    bool countEditing = false;     // champ numérique en cours de saisie
};

// Durées d'une image, en ms.
struct FrameRecord
{
    float frameMs = 0.0f;          // image complète, attente de la cadence comprise
    float stepMs = 0.0f;           // pas de simulation, vu du processeur
    float drawMs = 0.0f;           // préparation du dessin
    StepTimings step;              // décomposition du pas
};

// Les dernières images, de la plus ancienne à la plus récente.
class FrameHistory
{
public:
    static constexpr int kCapacity = 120;

    void push(const FrameRecord& record)
    {
        m_records[m_next] = record;
        m_next = (m_next + 1) % kCapacity;
        if (m_size < kCapacity)
        {
            ++m_size;
        }
    }

    int size() const
    {
        return m_size;
    }

    // age 0 : la plus ancienne ; size() - 1 : la plus récente.
    const FrameRecord& at(int age) const
    {
        return m_records[(m_next - m_size + age + kCapacity) % kCapacity];
    }

private:
    FrameRecord m_records[kCapacity];
    int m_next = 0;
    int m_size = 0;
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
    bool cpuCollisionsSkipped = false; // trop de particules pour les collisions sur le processeur
    float directCollideMs = -1.0f; // carte : dernières collisions toutes paires
    float gridCollideMs = -1.0f;   // carte : dernières collisions par la grille (construction comprise)
    float energy = -1.0f;          // énergie cinétique moyenne, calculée par le moteur actif
    float controlEnergy = -1.0f;   // même grandeur recalculée sur le processeur (touche V)
    const FrameHistory* history = nullptr;
    const char* message = nullptr; // information ponctuelle (export réussi…)
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
