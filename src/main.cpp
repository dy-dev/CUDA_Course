#include "Params.h"
#include "Particles.h"
#include "SimGpu.h"
#include "Ui.h"

#include "raylib.h"

#include <random>
#include <vector>

namespace
{
    const Color kBackground = { 18, 18, 24, 255 };
    const Color kParticleColor = { 120, 200, 255, 255 };

    // Positions uniformes dans le monde, vitesses uniformes dans [-v, v].
    void initParticles(std::vector<Particle>& particles, int count)
    {
        std::mt19937 rng(kSeed);
        std::uniform_real_distribution<float> randomX(0.0f, kWorldWidth);
        std::uniform_real_distribution<float> randomY(0.0f, kWorldHeight);
        std::uniform_real_distribution<float> randomSpeed(-kInitialSpeed, kInitialSpeed);

        particles.resize(count);
        for (Particle& p : particles)
        {
            p.x = randomX(rng);
            p.y = randomY(rng);
            p.vx = randomSpeed(rng);
            p.vy = randomSpeed(rng);
        }
    }

    // Avance chaque particule d'un pas de temps : position += vitesse × dt.
    void moveParticles(std::vector<Particle>& particles, float dt)
    {
        for (std::size_t i = 0; i < particles.size(); ++i)
        {
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
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

    void drawParticles(const std::vector<Particle>& particles, Texture2D disc)
    {
        BeginScissorMode(kPanelWidth, 0, kWindowWidth - kPanelWidth, kWindowHeight);

        const Rectangle source = { 0.0f, 0.0f, float(disc.width), float(disc.height) };
        const float size = 2.0f * kRadius;
        for (const Particle& p : particles)
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

    // Calcul confié à la carte graphique ; faux pour revenir à la boucle processeur.
    bool useGpu = true;

    std::vector<Particle> particles;
    initParticles(particles, ui.count);
    gpuInit(ui.count);

    while (!WindowShouldClose())
    {
        handleShortcuts(ui);
        if (ui.resetRequested || ui.count != int(particles.size()))
        {
            initParticles(particles, ui.count);
            gpuShutdown();
            gpuInit(ui.count);
            ui.resetRequested = false;
        }

        if (useGpu)
        {
            gpuStep(particles, kDt);
        }
        else
        {
            moveParticles(particles, kDt);
        }

        UiStats stats;
        stats.fps = GetFPS();
        stats.count = int(particles.size());
        stats.engine = useGpu ? "GPU" : "CPU";

        BeginDrawing();
        ClearBackground(kBackground);
        drawParticles(particles, disc);
        drawPanel(ui, stats);
        EndDrawing();
    }

    gpuShutdown();
    UnloadTexture(disc);
    CloseWindow();
    return 0;
}
