#include "Params.h"
#include "Particles.h"
#include "SimCpu.h"
#include "Simulation.h"
#include "Ui.h"
#if defined(CUDA_PROJECT_GPU)
#include "SimGpu.h"
#endif

#include "raylib.h"

#include <chrono>
#include <random>
#include <vector>

namespace
{
    const Color kBackground = { 18, 18, 24, 255 };
    const Color kParticleColor = { 120, 200, 255, 255 };

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

    void drawParticles(const std::vector<float2>& positions, Texture2D disc)
    {
        BeginScissorMode(kPanelWidth, 0, kWindowWidth - kPanelWidth, kWindowHeight);

        const Rectangle source = { 0.0f, 0.0f, float(disc.width), float(disc.height) };
        const float size = 2.0f * kRadius;
        for (const float2& p : positions)
        {
            const Rectangle dest = { p.x + float(kPanelWidth) - kRadius, p.y - kRadius, size, size };
            DrawTexturePro(disc, source, dest, { 0.0f, 0.0f }, 0.0f, kParticleColor);
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
    struct MouseTracker
    {
        bool pressedInWorld = false;
        double pressTime = 0.0;
    };

    MouseForce readMouse(MouseTracker& tracker, const UiState& ui)
    {
        MouseForce force;
        const Vector2 screen = GetMousePosition();
        force.pos = { screen.x - float(kPanelWidth), screen.y };

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
        const Color color = force.mode == MouseMode::Explode ? ORANGE : GREEN;
        DrawCircleLinesV(center, force.radius, color);
    }

    float elapsedMs(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
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
        handleShortcuts(ui);
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
        const auto stepStart = std::chrono::steady_clock::now();
        sim.step(params);
        lastStepMs[active] = elapsedMs(stepStart);

        UiStats stats;
        stats.fps = GetFPS();
        stats.count = initial.count();
        stats.engine = sim.name();
        stats.otherEngine = engines[1 - active] != nullptr ? engines[1 - active]->name() : "GPU";
        stats.stepMs = lastStepMs[active];
        stats.otherStepMs = lastStepMs[1 - active];
        stats.cpuCollisionsSkipped = active == 0 && ui.collisions && initial.count() > kCpuCollisionLimit;
#if defined(CUDA_PROJECT_GPU)
        if (active == 1)
        {
            stats.computeMs = gpu.computeMs();
            stats.copyMs = gpu.copyMs();
        }
#endif

        BeginDrawing();
        ClearBackground(kBackground);
        drawParticles(sim.positions(), disc);
        drawMouseForce(params.mouse);
        drawPanel(ui, stats);
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
