#include "Params.h"
#include "Particles.h"
#include "SimCpu.h"
#include "Simulation.h"
#include "Timing.h"
#include "Ui.h"
#if defined(CUDA_PROJECT_GPU)
#include "SimGpu.h"
#endif

#include "raylib.h"

#include <cstdio>
#include <random>
#include <vector>

namespace
{
    const Color kBackground = { 18, 18, 24, 255 };
    const Color kParticleColor = { 120, 200, 255, 255 };

    // Une couleur selon la parité des coordonnées de cellule : damier de 2 × 2.
    const Color kCellColors[4] = { { 120, 200, 255, 255 }, { 255, 170, 80, 255 }, { 140, 230, 120, 255 }, { 240, 110, 160, 255 } };

    Color cellColor(int cell)
    {
        const int cx = cell % kGridWidth;
        const int cy = cell / kGridWidth;
        return kCellColors[(cx & 1) + 2 * (cy & 1)];
    }

    // Positions uniformes dans le monde, vitesses uniformes dans [-v, v].
    void initParticles(Particles& particles, int count)
    {
        std::mt19937 rng(kSeed);
        std::uniform_real_distribution<float> randomX(0.0f, kWorldWidth);
        std::uniform_real_distribution<float> randomY(0.0f, kWorldHeight);
        std::uniform_real_distribution<float> randomSpeed(-kInitialSpeed, kInitialSpeed);

        particles.pos.resize(count);
        particles.vel.resize(count);
        for (int i = 0; i < count; ++i)
        {
            particles.pos[i] = { randomX(rng), randomY(rng) };
            particles.vel[i] = { randomSpeed(rng), randomSpeed(rng) };
        }
    }

    // Disque blanc lissé, teinté au dessin. Une particule = un quad texturé :
    // bien moins coûteux qu'un disque tracé en triangles (DrawCircleV).
    Texture2D makeDiscTexture()
    {
        Image image = GenImageColor(8, 8, BLANK);
        ImageDrawCircleV(&image, { 4.0f, 4.0f }, 4, WHITE);
        Texture2D texture = LoadTextureFromImage(image);
        UnloadImage(image);
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
        return texture;
    }

    void drawParticles(const std::vector<float2>& positions, const std::vector<int>* cellIds, Texture2D disc)
    {
        BeginScissorMode(kPanelWidth, 0, kWindowWidth - kPanelWidth, kWindowHeight);

        const Rectangle source = { 0.0f, 0.0f, float(disc.width), float(disc.height) };
        const float size = 2.0f * kRadius;
        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            const float2 p = positions[i];
            const Rectangle dest = { p.x + float(kPanelWidth) - kRadius, p.y - kRadius, size, size };
            const Color color = cellIds != nullptr ? cellColor((*cellIds)[i]) : kParticleColor;
            DrawTexturePro(disc, source, dest, { 0.0f, 0.0f }, 0.0f, color);
        }

        EndScissorMode();
    }

    void handleShortcuts(UiState& ui)
    {
        if (isTyping(ui))
        {
            return;
        }
        for (int k = 0; k < 5; ++k)
        {
            if (IsKeyPressed(KEY_ONE + k))
            {
                requestCount(ui, kPresetCounts[k]);
            }
        }
        if (IsKeyPressed(KEY_G) && ui.gpuAvailable)
        {
            ui.useGpu = !ui.useGpu;
        }
        if (IsKeyPressed(KEY_C))
        {
            ui.colorByCell = !ui.colorByCell;
        }
        if (IsKeyPressed(KEY_T))
        {
            ui.collisionMode = ui.collisionMode == 3 ? 2 : 3;
        }
        if (IsKeyPressed(KEY_V))
        {
            ui.verifyRequested = true;
        }
        if (IsKeyPressed(KEY_M))
        {
            ui.exportRequested = true;
        }
    }

    // Passe d'un moteur à l'autre en conservant l'état des particules.
    void switchEngine(Simulation& from, Simulation& to)
    {
        Particles state;
        from.readState(state);
        to.shutdown();
        to.init(state);
    }

    // Suivi du bouton gauche : un clic bref déclenche une explosion au relâchement,
    // un bouton maintenu attire les particules. Les clics sur le panneau sont ignorés.
    // Bonus : le bouton droit maintenu fait tourbillonner les particules.
    struct MouseTracker
    {
        bool pressedInWorld = false;
        bool swirlInWorld = false;
        double pressTime = 0.0;
    };

    MouseForce readMouse(MouseTracker& tracker, const UiState& ui)
    {
        MouseForce force;
        const Vector2 screen = GetMousePosition();
        force.pos = { screen.x - float(kPanelWidth), screen.y };

        // Tourbillon : même rayon et même intensité que l'attraction. Le bouton
        // gauche, s'il agit aussi, l'emporte.
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            tracker.swirlInWorld = !isOverPanel(screen);
        }
        if (tracker.swirlInWorld && IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            force.mode = MouseMode::Swirl;
            force.strength = ui.attraction;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            tracker.pressedInWorld = !isOverPanel(screen);
            tracker.pressTime = GetTime();
        }
        if (!tracker.pressedInWorld)
        {
            return force;
        }

        const double held = GetTime() - tracker.pressTime;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            tracker.pressedInWorld = false;
            if (held < kClickMaxSeconds)
            {
                force.mode = MouseMode::Explode;
                force.strength = ui.explosion;
            }
        }
        else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && held >= kClickMaxSeconds)
        {
            force.mode = MouseMode::Attract;
            force.strength = ui.attraction;
        }
        return force;
    }

    void drawMouseForce(const MouseForce& force)
    {
        if (force.mode == MouseMode::None)
        {
            return;
        }
        const Vector2 center = { force.pos.x + float(kPanelWidth), force.pos.y };
        const Color color = force.mode == MouseMode::Explode ? ORANGE
                          : force.mode == MouseMode::Swirl   ? SKYBLUE
                                                             : GREEN;
        DrawCircleLinesV(center, force.radius, color);
    }

    // Contrôle : énergie cinétique moyenne recalculée sur le processeur à partir de
    // l'état complet du moteur actif. Cumul en double : c'est la valeur de référence.
    float controlMeanKineticEnergy(Simulation& sim)
    {
        Particles state;
        sim.readState(state);
        double sum = 0.0;
        for (const float2& v : state.vel)
        {
            sum += 0.5 * (double(v.x) * v.x + double(v.y) * v.y);
        }
        return state.count() > 0 ? float(sum / state.count()) : 0.0f;
    }

    // Écrit l'historique des durées d'image dans le dossier courant.
    bool exportCsv(const FrameHistory& history, const char* engine, int count, const char* path)
    {
        FILE* file = std::fopen(path, "w");
        if (file == nullptr)
        {
            return false;
        }

        std::fprintf(file, "image,moteur,n,ms_image,ms_pas,ms_souris,ms_collisions,ms_deplacement,ms_copie,ms_dessin\n");
        for (int age = 0; age < history.size(); ++age)
        {
            const FrameRecord& r = history.at(age);
            std::fprintf(file, "%d,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", age, engine, count,
                         r.frameMs, r.stepMs, r.step.mouseMs, r.step.collideMs, r.step.moveMs, r.step.copyMs, r.drawMs);
        }
        std::fclose(file);
        return true;
    }
}

int main()
{
    InitWindow(kWindowWidth, kWindowHeight, "Particules");
    SetTargetFPS(60);
    initUi();
    const Texture2D disc = makeDiscTexture();

    UiState ui;
    requestCount(ui, kDefaultCount);
    ui.restitution = kDefaultRestitution;
    ui.explosion = kDefaultExplosion;
    ui.attraction = kDefaultAttraction;
    MouseTracker mouseTracker;

    // Moteurs : 0 processeur, 1 carte graphique (absent s'il n'est pas construit).
    SimCpu cpu;
#if defined(CUDA_PROJECT_GPU)
    SimGpu gpu;
    Simulation* engines[2] = { &cpu, &gpu };
#else
    Simulation* engines[2] = { &cpu, nullptr };
    ui.gpuAvailable = false;
    ui.useGpu = false;
#endif
    float lastStepMs[2] = { -1.0f, -1.0f };
    int active = ui.useGpu ? 1 : 0;

    float controlEnergy = -1.0f;
    double controlUntil = 0.0;
    float directCollideMs = -1.0f;
    float gridCollideMs = -1.0f;
    float directTiledCollideMs = -1.0f;
    float gridOnlyMs = -1.0f;
    float tiledOnlyMs = -1.0f;

    FrameHistory history;
    FrameRecord current;
    const char* message = nullptr;
    double messageUntil = 0.0;

    Particles initial;
    initParticles(initial, ui.count);
    for (Simulation* engine : engines)
    {
        if (engine != nullptr)
        {
            engine->init(initial);
        }
    }

    while (!WindowShouldClose())
    {
        // La durée de l'image précédente n'est connue qu'au début de celle-ci.
        if (GetTime() > 0.5)
        {
            current.frameMs = GetFrameTime() * 1000.0f;
            history.push(current);
        }

        handleShortcuts(ui);
        if (ui.exportRequested)
        {
            const char* path = "frame_times.csv";
            message = exportCsv(history, engines[active]->name(), initial.count(), path)
                ? "Exporté : frame_times.csv" : "Échec de l'export CSV";
            messageUntil = GetTime() + 3.0;
            ui.exportRequested = false;
        }
        if (ui.resetRequested || ui.count != initial.count())
        {
            initParticles(initial, ui.count);
            for (Simulation* engine : engines)
            {
                if (engine != nullptr)
                {
                    engine->shutdown();
                    engine->init(initial);
                }
            }
            lastStepMs[0] = lastStepMs[1] = -1.0f;
            directCollideMs = directTiledCollideMs = gridCollideMs = gridOnlyMs = tiledOnlyMs = -1.0f;
            ui.resetRequested = false;
        }

        const int wanted = ui.useGpu ? 1 : 0;
        if (wanted != active)
        {
            switchEngine(*engines[active], *engines[wanted]);
            active = wanted;
        }
        Simulation& sim = *engines[active];

        StepParams params;
        params.walls = ui.walls;
        params.restitution = ui.restitution;
        params.collisions = ui.collisions;
        params.mouse = readMouse(mouseTracker, ui);
        const CollisionMode modes[4] = { CollisionMode::Direct, CollisionMode::DirectTiled, CollisionMode::Grid, CollisionMode::GridTiled };
        params.collisionMode = modes[ui.collisionMode];
        params.needCellIds = ui.colorByCell;
        CpuTimer stepTimer;
        sim.step(params);
        lastStepMs[active] = stepTimer.elapsedMs();
        current.stepMs = lastStepMs[active];
        current.step = sim.timings();

        // Dernières durées de collision de chaque méthode, sur la carte.
        if (active == 1 && params.collisions)
        {
            if (params.collisionMode == CollisionMode::Direct)
            {
                directCollideMs = current.step.collideMs;
            }
            else if (params.collisionMode == CollisionMode::DirectTiled)
            {
                directTiledCollideMs = current.step.collideMs;
            }
            else
            {
                gridCollideMs = current.step.gridMs + current.step.collideMs;
                // Construction de la grille identique : seule la recherche des voisines diffère.
                float& onlyMs = params.collisionMode == CollisionMode::Grid ? gridOnlyMs : tiledOnlyMs;
                onlyMs = current.step.collideMs;
            }
        }

        UiStats stats;
        stats.fps = GetFPS();
        stats.count = initial.count();
        stats.engine = sim.name();
        stats.otherEngine = engines[1 - active] != nullptr ? engines[1 - active]->name() : "GPU";
        stats.stepMs = lastStepMs[active];
        stats.otherStepMs = lastStepMs[1 - active];
        stats.cpuCollisionsSkipped = active == 0 && ui.collisions && initial.count() > kCpuCollisionLimit;
        stats.energy = sim.meanKineticEnergy();
        if (ui.verifyRequested)
        {
            controlEnergy = controlMeanKineticEnergy(sim);
            controlUntil = GetTime() + 5.0;
            ui.verifyRequested = false;
        }
        stats.controlEnergy = GetTime() < controlUntil ? controlEnergy : -1.0f;
        stats.directCollideMs = directCollideMs;
        stats.directTiledCollideMs = directTiledCollideMs;
        stats.gridOnlyMs = gridOnlyMs;
        stats.tiledOnlyMs = tiledOnlyMs;
        stats.gridCollideMs = gridCollideMs;
        stats.history = &history;
        stats.message = GetTime() < messageUntil ? message : nullptr;

        BeginDrawing();
        CpuTimer drawTimer;
        ClearBackground(kBackground);
        drawParticles(sim.positions(), ui.colorByCell ? sim.cellIds() : nullptr, disc);
        drawMouseForce(params.mouse);
        drawPanel(ui, stats);
        current.drawMs = drawTimer.elapsedMs();
        EndDrawing();
    }

    for (Simulation* engine : engines)
    {
        if (engine != nullptr)
        {
            engine->shutdown();
        }
    }
    UnloadTexture(disc);
    CloseWindow();
    return 0;
}
