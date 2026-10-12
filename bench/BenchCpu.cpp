// Mesure console : durée d'un pas de déplacement sur le processeur,
// pour plusieurs tailles de population.

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <vector>

namespace
{
    struct Particle
    {
        float x, y;
        float vx, vy;
    };

    constexpr int kSizes[] = { 1000, 10000, 100000, 1000000 };
    constexpr int kSteps = 100;
    constexpr float kDt = 1.0f / 60.0f;
    constexpr float kFrameBudgetMs = 1000.0f / 60.0f;
    constexpr const char* kCsvPath = "cpu_ms.csv";

    void initParticles(std::vector<Particle>& particles, int count)
    {
        std::mt19937 rng(1234);
        std::uniform_real_distribution<float> randomPos(0.0f, 1000.0f);
        std::uniform_real_distribution<float> randomSpeed(-60.0f, 60.0f);

        particles.resize(count);
        for (Particle& p : particles)
        {
            p.x = randomPos(rng);
            p.y = randomPos(rng);
            p.vx = randomSpeed(rng);
            p.vy = randomSpeed(rng);
        }
    }

    void moveParticles(std::vector<Particle>& particles, float dt)
    {
        for (std::size_t i = 0; i < particles.size(); ++i)
        {
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
        }
    }
}

int main()
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::ofstream csv(kCsvPath);
    csv << "n,ms_per_step\n";

    std::printf("Déplacement CPU, %d pas par taille\n\n", kSteps);
    std::printf("%10s  %12s  %14s  %10s\n", "N", "ms par pas", "images/s max", "budget");

    for (int n : kSizes)
    {
        std::vector<Particle> particles;
        initParticles(particles, n);

        // Pas à blanc : met les données en cache avant la mesure.
        moveParticles(particles, kDt);

        const auto start = std::chrono::steady_clock::now();
        for (int step = 0; step < kSteps; ++step)
        {
            moveParticles(particles, kDt);
        }
        const auto stop = std::chrono::steady_clock::now();
        const double msPerStep = std::chrono::duration<double, std::milli>(stop - start).count() / kSteps;

        const double maxFps = msPerStep > 0.0 ? 1000.0 / msPerStep : 0.0;
        const bool withinBudget = msPerStep <= kFrameBudgetMs;
        std::printf("%10d  %12.4f  %14.0f  %10s\n", n, msPerStep, maxFps, withinBudget ? "tenu" : "dépassé");

        // Affichage d'une valeur calculée : empêche le compilateur d'éliminer la boucle.
        std::printf("%10s  contrôle x[0] = %.3f\n", "", particles[0].x);

        csv << n << ',' << msPerStep << '\n';
    }

    std::printf("\nRésultats écrits dans %s\n", kCsvPath);
    return 0;
}
