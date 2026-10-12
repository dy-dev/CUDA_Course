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
    const int i = blockIdx.x * blockDim.x + threadIdx.x;

    // Le dernier bloc est en partie hors du tableau.
    if (i >= n)
    {
        return;
    }

    d_particles[i].x += d_particles[i].vx * dt;
    d_particles[i].y += d_particles[i].vy * dt;
}

// Lecture de la seule coordonnée x, selon deux organisations des données.
__global__ void readXAosKernel(const Particle* d_particles, float* d_out, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        d_out[i] = d_particles[i].x;
    }
}

__global__ void readXSoaKernel(const float* d_x, float* d_out, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
    {
        d_out[i] = d_x[i];
    }
}

namespace
{
    constexpr int kLayoutSizes[] = { 1000000, 10000000 };

    // Durée moyenne d'un lancement, mesurée par événements CUDA.
    template <typename Launch>
    float timeKernel(Launch launch)
    {
        cudaEvent_t start;
        cudaEvent_t stop;
        cudaEventCreate(&start);
        cudaEventCreate(&stop);

        launch();   // pas à blanc
        cudaEventRecord(start);
        for (int step = 0; step < kSteps; ++step)
        {
            launch();
        }
        cudaEventRecord(stop);
        cudaEventSynchronize(stop);

        float ms = 0.0f;
        cudaEventElapsedTime(&ms, start, stop);
        cudaEventDestroy(start);
        cudaEventDestroy(stop);
        return ms / kSteps;
    }

    // Tableau de structures contre structure de tableaux : seul x est lu.
    // En AoS, chaque lecture de 4 octets entraîne celle de toute la particule (16 octets).
    void compareLayouts()
    {
        std::printf("\nLecture de x seul : tableau de structures (AoS) contre structure de tableaux (SoA)\n\n");
        std::printf("%10s  %12s  %12s  %s\n", "N", "ms AoS", "ms SoA", "facteur SoA/AoS");

        for (int n : kLayoutSizes)
        {
            std::vector<Particle> h_particles;
            initParticles(h_particles, n);
            std::vector<float> h_x(n);
            for (int i = 0; i < n; ++i)
            {
                h_x[i] = h_particles[i].x;
            }

            Particle* d_particles = nullptr;
            float* d_x = nullptr;
            float* d_out = nullptr;
            cudaMalloc(&d_particles, n * sizeof(Particle));
            cudaMalloc(&d_x, n * sizeof(float));
            cudaMalloc(&d_out, n * sizeof(float));
            cudaMemcpy(d_particles, h_particles.data(), n * sizeof(Particle), cudaMemcpyHostToDevice);
            cudaMemcpy(d_x, h_x.data(), n * sizeof(float), cudaMemcpyHostToDevice);

            const int gridSize = (n + kBlockSize - 1) / kBlockSize;
            const float aosMs = timeKernel([&] { readXAosKernel<<<gridSize, kBlockSize>>>(d_particles, d_out, n); });
            const float soaMs = timeKernel([&] { readXSoaKernel<<<gridSize, kBlockSize>>>(d_x, d_out, n); });
            std::printf("%10d  %12.4f  %12.4f  %.2fx\n", n, aosMs, soaMs, aosMs / soaMs);

            cudaFree(d_particles);
            cudaFree(d_x);
            cudaFree(d_out);
        }
    }
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
    std::printf("%10s  %8s  %12s  %12s  %12s  %s\n", "N", "blocs", "ms GPU", "ms CPU", "écart rel.", "facteur");

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

        // Arrondi supérieur : un bloc de plus si n n'est pas multiple de kBlockSize.
        const int gridSize = (n + kBlockSize - 1) / kBlockSize;

        // Pas à blanc : le premier lancement paie le chargement du kernel.
        moveParticlesKernel<<<gridSize, kBlockSize>>>(d_particles, n, kDt);
        cudaDeviceSynchronize();

        const auto start = std::chrono::steady_clock::now();
        for (int step = 0; step < kSteps; ++step)
        {
            moveParticlesKernel<<<gridSize, kBlockSize>>>(d_particles, n, kDt);
        }
        // Les lancements sont asynchrones : sans cette attente, on ne mesurerait que leur mise en file.
        cudaDeviceSynchronize();
        const auto stop = std::chrono::steady_clock::now();
        const double gpuMs = std::chrono::duration<double, std::milli>(stop - start).count() / kSteps;

        cudaMemcpy(h_particles.data(), d_particles, n * sizeof(Particle), cudaMemcpyDeviceToHost);
        cudaFree(d_particles);

        // Écart relatif : plus grand écart rapporté à la plus grande coordonnée. La carte
        // fusionne multiplication et addition (FMA), ce qui change l'arrondi de chaque pas.
        float maxDiff = 0.0f;
        float maxCoord = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            maxDiff = std::max(maxDiff, std::fabs(h_particles[i].x - reference[i].x));
            maxDiff = std::max(maxDiff, std::fabs(h_particles[i].y - reference[i].y));
            maxCoord = std::max(maxCoord, std::max(std::fabs(reference[i].x), std::fabs(reference[i].y)));
        }
        const float maxError = maxDiff / maxCoord;

        const auto cpu = cpuTimes.find(n);
        const double cpuMs = cpu != cpuTimes.end() ? cpu->second : 0.0;
        std::printf("%10d  %8d  %12.4f  %12.4f  %12.2e  ", n, gridSize, gpuMs, cpuMs, maxError);
        printFactor(cpuMs, gpuMs);
        std::printf("\n");
    }

    compareLayouts();
    return 0;
}
