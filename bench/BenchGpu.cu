// Mesure console : déplacement des particules sur la carte graphique,
// comparé aux durées CPU enregistrées par bench_cpu dans cpu_ms.csv.

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cuda_runtime.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <string>
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
    constexpr int kBlockSize = 256;
    constexpr const char* kCpuCsvPath = "cpu_ms.csv";

    // Mêmes données de départ que bench_cpu.
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

    // Version processeur, sert de référence pour vérifier les résultats de la carte.
    void moveParticlesCpu(std::vector<Particle>& particles, float dt)
    {
        for (std::size_t i = 0; i < particles.size(); ++i)
        {
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;
        }
    }

    // Durée CPU par pas, indexée par taille. Vide si le fichier est absent.
    std::map<int, double> readCpuTimes(const char* path)
    {
        std::map<int, double> times;
        std::ifstream file(path);
        std::string header;
        std::getline(file, header);

        int n = 0;
        char comma = 0;
        double ms = 0.0;
        while (file >> n >> comma >> ms)
        {
            times[n] = ms;
        }
        return times;
    }

    // En dessous, la durée mesurée n'est que le coût de la boucle : pas de facteur.
    constexpr double kMinMeasurableMs = 1e-4;

    void printFactor(double cpuMs, double gpuMs)
    {
        if (cpuMs < kMinMeasurableMs || gpuMs < kMinMeasurableMs)
        {
            std::printf("%s", "-");
        }
        else if (gpuMs < cpuMs)
        {
            std::printf("GPU %.1fx plus rapide", cpuMs / gpuMs);
        }
        else
        {
            std::printf("CPU %.1fx plus rapide", gpuMs / cpuMs);
        }
    }
}

__global__ void moveParticlesKernel(Particle* d_particles, int n, float dt)
{
    // ___TODO___ : calculer l'indice global du thread à partir de son bloc et de sa place dans le bloc
    int i = 0;

    // ___TODO___ : ne rien faire si l'indice dépasse le nombre de particules

    d_particles[i].x += d_particles[i].vx * dt;
    d_particles[i].y += d_particles[i].vy * dt;
}

int main()
{
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    std::printf("Carte : %s\n", prop.name);

    const std::map<int, double> cpuTimes = readCpuTimes(kCpuCsvPath);
    if (cpuTimes.empty())
    {
        std::printf("%s introuvable : lancer bench_cpu d'abord, depuis le même dossier.\n", kCpuCsvPath);
    }

    std::printf("Déplacement GPU, %d pas par taille, blocs de %d threads\n\n", kSteps, kBlockSize);
    std::printf("%10s  %8s  %12s  %12s  %12s  %s\n", "N", "blocs", "ms GPU", "ms CPU", "écart max", "facteur");

    for (int n : kSizes)
    {
        std::vector<Particle> h_particles;
        initParticles(h_particles, n);

        // Référence : pas à blanc + pas mesurés, comme sur la carte.
        std::vector<Particle> reference = h_particles;
        for (int step = 0; step < kSteps + 1; ++step)
        {
            moveParticlesCpu(reference, kDt);
        }

        Particle* d_particles = nullptr;
        cudaMalloc(&d_particles, n * sizeof(Particle));
        cudaMemcpy(d_particles, h_particles.data(), n * sizeof(Particle), cudaMemcpyHostToDevice);

        // ___TODO___ : calculer le nombre de blocs nécessaire pour couvrir les n particules (arrondi supérieur)
        int gridSize = 0;

        // Pas à blanc : le premier lancement paie le chargement du kernel.
        // ___TODO___ : lancer moveParticlesKernel sur gridSize blocs de kBlockSize threads
        cudaDeviceSynchronize();

        const auto start = std::chrono::steady_clock::now();
        for (int step = 0; step < kSteps; ++step)
        {
            // ___TODO___ : lancer moveParticlesKernel sur gridSize blocs de kBlockSize threads
        }
        // ___TODO___ : attendre que la carte ait terminé avant d'arrêter le chronomètre
        const auto stop = std::chrono::steady_clock::now();
        const double gpuMs = std::chrono::duration<double, std::milli>(stop - start).count() / kSteps;

        cudaMemcpy(h_particles.data(), d_particles, n * sizeof(Particle), cudaMemcpyDeviceToHost);
        cudaFree(d_particles);

        float maxError = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            maxError = std::max(maxError, std::fabs(h_particles[i].x - reference[i].x));
            maxError = std::max(maxError, std::fabs(h_particles[i].y - reference[i].y));
        }

        const auto cpu = cpuTimes.find(n);
        const double cpuMs = cpu != cpuTimes.end() ? cpu->second : 0.0;
        std::printf("%10d  %8d  %12.4f  %12.4f  %12.2e  ", n, gridSize, gpuMs, cpuMs, maxError);
        printFactor(cpuMs, gpuMs);
        std::printf("\n");
    }

    return 0;
}
