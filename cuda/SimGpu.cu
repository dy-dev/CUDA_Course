#include "SimGpu.h"

#include "CudaCheck.h"
#include "Params.h"
#include "Physics.h"

#include <utility>

namespace
{
    // Démonstration : blocs de 2048 threads, au-delà de la limite de la carte (1024).
    // Le lancement est refusé ; seule la vérification d'erreurs le signale.
    constexpr bool kDemoBadLaunch = false;

    // Nombre de blocs pour couvrir n éléments (arrondi supérieur).
    int gridFor(int n, int blockSize)
    {
        return (n + blockSize - 1) / blockSize;
    }
}

// Déplacement d'un pas de temps, puis rebond éventuel sur les murs, par les
// fonctions partagées avec le moteur CPU.
__global__ void moveParticlesKernel(float2* d_pos, float2* d_vel, int n, float dt, bool walls, float restitution)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    float2 p = d_pos[i];
    float2 v = d_vel[i];
    advance(p, v, dt);
    if (walls)
    {
        bounce(p, v, restitution);
    }
    d_pos[i] = p;
    d_vel[i] = v;
}

// Force exercée par la souris : explosion (une seule fois), attraction ou tourbillon
// (continus), par la fonction partagée avec le moteur CPU.
// Les réglages arrivent par valeur, avec les autres paramètres du lancement.
__global__ void mouseForceKernel(const float2* d_pos, float2* d_vel, int n, MouseForce mouse, float dt)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    float2 v = d_vel[i];
    applyMouseForce(d_pos[i], v, mouse, dt);
    d_vel[i] = v;
}

// Collisions, toutes les paires. Chaque thread traite une particule et parcourt
// toutes les autres. Il lit l'état d'entrée et n'écrit que dans les tableaux de
// sortie (double tampon) : aucun thread n'écrit ce qu'un autre lit.
// Il en sort la position séparée de ses voisines et le partenaire choisi pour
// l'échange de vitesse : la voisine la plus proche parmi celles qui se rapprochent.
__global__ void collideAllPairsKernel(const float2* d_pos, const float2* d_vel,
                                      float2* d_posOut, int* d_partner, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    const float minDist = 2.0f * kRadius;

    float2 p = d_pos[i];
    const float2 v = d_vel[i];

    // Séparation cumulée sur toutes les voisines, appliquée à la fin : chaque
    // paire est vue depuis l'état d'entrée, comme par l'autre particule.
    float2 dp = { 0.0f, 0.0f };
    int partner = -1;
    float partnerDist = minDist;
    for (int j = 0; j < n; ++j)
    {
        float2 normal;
        float dist;
        if (j == i || !touching(p, d_pos[j], normal, dist))
        {
            continue;
        }

        // Séparation : la particule recule de la moitié du chevauchement.
        const float half = 0.5f * (minDist - dist);
        dp.x -= normal.x * half;
        dp.y -= normal.y * half;

        // Partenaire : la plus proche des voisines qui se rapprochent.
        const float2 vj = d_vel[j];
        const float vn = (vj.x - v.x) * normal.x + (vj.y - v.y) * normal.y;
        if (vn < 0.0f && dist < partnerDist)
        {
            partner = j;
            partnerDist = dist;
        }
    }

    p.x += dp.x;
    p.y += dp.y;
    d_posOut[i] = p;
    d_partner[i] = partner;
}

// Collisions, toutes les paires, par tuiles. Un thread par particule, comme
// ci-dessus ; mais au lieu de relire toutes les autres en mémoire globale, le
// bloc les charge par tuiles de blockDim.x en mémoire partagée : chaque thread
// en charge une, puis tous comparent leur particule à la tuile entière.
__global__ void collideAllPairsTiledKernel(const float2* d_pos, const float2* d_vel,
                                           float2* d_posOut, int* d_partner, int n)
{
    __shared__ float2 s_pos[kBlockSize];
    __shared__ float2 s_vel[kBlockSize];

    // Garde de bornes : un thread sans particule ne quitte pas le kernel, il
    // participera aux chargements et aux barrières sans rien calculer.
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    const bool active = i < n;
    const float2 p = active ? d_pos[i] : make_float2(0.0f, 0.0f);
    const float2 v = active ? d_vel[i] : make_float2(0.0f, 0.0f);
    float2 dp = { 0.0f, 0.0f };
    int partner = -1;
    float partnerDist = 2.0f * kRadius;

    // ___TODO___ : parcourir toutes les particules par tuiles de blockDim.x : chaque thread charge une particule de la tuile (s'il en reste) dans s_pos et s_vel, barrière, puis les threads actifs appellent accumulateContact pour chaque particule de la tuile autre que la leur, barrière avant la tuile suivante

    if (active)
    {
        d_posOut[i] = make_float2(p.x + dp.x, p.y + dp.y);
        d_partner[i] = partner;
    }
}

// Échange des composantes normales de vitesse entre partenaires mutuels (i a
// choisi j et j a choisi i). Chaque particule échange avec une seule autre :
// l'énergie et la quantité de mouvement sont conservées. Les contacts non
// retenus seront traités aux pas suivants.
__global__ void exchangeVelocitiesKernel(const float2* d_pos, const float2* d_vel,
                                         const int* d_partner, float2* d_velOut, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    float2 v = d_vel[i];
    const int j = d_partner[i];
    float2 normal;
    float dist;
    if (j >= 0 && d_partner[j] == i && touching(d_pos[i], d_pos[j], normal, dist))
    {
        const float2 vj = d_vel[j];
        const float vn = (vj.x - v.x) * normal.x + (vj.y - v.y) * normal.y;
        v.x += vn * normal.x;
        v.y += vn * normal.y;
    }
    d_velOut[i] = v;
}

// Somme par bloc de l'énergie cinétique ½|v|², en mémoire partagée : chaque
// bloc produit un seul nombre, son total.
__global__ void kineticEnergyBlockSumKernel(const float2* d_vel, float* d_blockSums, int n)
{
    __shared__ float s_sum[kBlockSize];
    const int tid = threadIdx.x;
    const int i = blockIdx.x * blockDim.x + tid;

    // Tous les threads écrivent leur case, même au-delà du tableau (avec 0) :
    // ainsi tous atteignent les barrières.
    const float2 v = i < n ? d_vel[i] : make_float2(0.0f, 0.0f);
    s_sum[tid] = 0.5f * (v.x * v.x + v.y * v.y);
    __syncthreads();

    // À chaque étape, la première moitié des cases reçoit la seconde.
    for (int stride = blockDim.x / 2; stride > 0; stride /= 2)
    {
        if (tid < stride)
        {
            s_sum[tid] += s_sum[tid + stride];
        }
        __syncthreads();
    }

    if (tid == 0)
    {
        d_blockSums[blockIdx.x] = s_sum[0];
    }
}

// Somme finale des totaux de blocs, par un seul bloc : chaque thread cumule
// plusieurs valeurs, puis le bloc réduit ces cumuls en mémoire partagée.
__global__ void sumKernel(const float* d_values, float* d_total, int n)
{
    __shared__ float s_sum[kBlockSize];
    const int tid = threadIdx.x;

    float sum = 0.0f;
    for (int k = tid; k < n; k += blockDim.x)
    {
        sum += d_values[k];
    }
    s_sum[tid] = sum;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride /= 2)
    {
        if (tid < stride)
        {
            s_sum[tid] += s_sum[tid + stride];
        }
        __syncthreads();
    }

    if (tid == 0)
    {
        *d_total = s_sum[0];
    }
}

void SimGpu::init(const Particles& state)
{
    m_count = state.count();
    const size_t bytes = m_count * sizeof(float2);

    // Seule copie aller : les données restent ensuite sur la carte.
    CUDA_CHECK(cudaMalloc(&m_device.d_pos, bytes));
    CUDA_CHECK(cudaMalloc(&m_device.d_vel, bytes));
    CUDA_CHECK(cudaMalloc(&m_deviceOut.d_pos, bytes));
    CUDA_CHECK(cudaMalloc(&m_deviceOut.d_vel, bytes));
    CUDA_CHECK(cudaMalloc(&d_partner, m_count * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_blockSums, gridFor(m_count, kBlockSize) * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&d_total, sizeof(float)));
    m_grid.init(m_count);
    m_cellIds.resize(m_count);
    CUDA_CHECK(cudaMemcpy(m_device.d_pos, state.pos.data(), bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(m_device.d_vel, state.vel.data(), bytes, cudaMemcpyHostToDevice));
    m_positions = state.pos;

}

void SimGpu::step(const StepParams& params)
{
    const int gridSize = gridFor(m_count, kBlockSize);

    if (params.mouse.mode != MouseMode::None)
    {
        m_mouseTimer.start();
        mouseForceKernel<<<gridSize, kBlockSize>>>(m_device.d_pos, m_device.d_vel, m_count, params.mouse, params.dt);
        CUDA_CHECK_LAUNCH();
        m_mouseTimer.stop();
    }

    if (params.collisions)
    {
        if (params.collisionMode == CollisionMode::Direct)
        {
            m_collideTimer.start();
            collideAllPairsKernel<<<gridSize, kBlockSize>>>(
                m_device.d_pos, m_device.d_vel, m_deviceOut.d_pos, d_partner, m_count);
            CUDA_CHECK_LAUNCH();
        }
        else if (params.collisionMode == CollisionMode::DirectTiled)
        {
            m_collideTimer.start();
            collideAllPairsTiledKernel<<<gridSize, kBlockSize>>>(
                m_device.d_pos, m_device.d_vel, m_deviceOut.d_pos, d_partner, m_count);
            CUDA_CHECK_LAUNCH();
        }
        else
        {
            // Tri par cellule : l'ordre des particules change à chaque pas.
            m_gridTimer.start();
            m_grid.build(m_device, m_deviceOut, m_count);
            m_gridTimer.stop();

            m_collideTimer.start();
            if (params.collisionMode == CollisionMode::GridTiled)
            {
                m_grid.collideTiled(m_device, m_deviceOut.d_pos, d_partner);
            }
            else
            {
                m_grid.collide(m_device, m_deviceOut.d_pos, d_partner, m_count);
            }
        }
        exchangeVelocitiesKernel<<<gridSize, kBlockSize>>>(
            m_device.d_pos, m_device.d_vel, d_partner, m_deviceOut.d_vel, m_count);
        CUDA_CHECK_LAUNCH();
        m_collideTimer.stop();
        // Le résultat devient l'état courant ; l'ancien état servira de sortie au pas suivant.
        std::swap(m_device, m_deviceOut);
    }

    m_moveTimer.start();
    const int moveBlockSize = kDemoBadLaunch ? 2048 : kBlockSize;
    moveParticlesKernel<<<gridFor(m_count, moveBlockSize), moveBlockSize>>>(
        m_device.d_pos, m_device.d_vel, m_count, params.dt, params.walls, params.restitution);
    CUDA_CHECK_LAUNCH();
    m_moveTimer.stop();

    // Copie retour des seules positions, pour le dessin.
    m_copyTimer.start();
    CUDA_CHECK(cudaMemcpy(m_positions.data(), m_device.d_pos, m_count * sizeof(float2), cudaMemcpyDeviceToHost));
    // Cellules, pour la coloration : valables seulement si la grille vient d'être construite.
    const bool gridBuilt = params.collisionMode == CollisionMode::Grid || params.collisionMode == CollisionMode::GridTiled;
    m_hasCellIds = params.needCellIds && params.collisions && gridBuilt;
    if (m_hasCellIds)
    {
        CUDA_CHECK(cudaMemcpy(m_cellIds.data(), m_grid.cellIds(), m_count * sizeof(int), cudaMemcpyDeviceToHost));
    }
    m_copyTimer.stop();

    m_timings.mouseMs = m_mouseTimer.elapsedMs();
    m_timings.gridMs = m_gridTimer.elapsedMs();
    m_timings.collideMs = m_collideTimer.elapsedMs();
    m_timings.moveMs = m_moveTimer.elapsedMs();
    m_timings.copyMs = m_copyTimer.elapsedMs();
}

float SimGpu::meanKineticEnergy()
{
    const int blocks = gridFor(m_count, kBlockSize);
    CUDA_CHECK(cudaMemset(d_total, 0, sizeof(float)));

    kineticEnergyBlockSumKernel<<<blocks, kBlockSize>>>(m_device.d_vel, d_blockSums, m_count);
    CUDA_CHECK_LAUNCH();
    sumKernel<<<1, kBlockSize>>>(d_blockSums, d_total, blocks);
    CUDA_CHECK_LAUNCH();

    float total = 0.0f;
    CUDA_CHECK(cudaMemcpy(&total, d_total, sizeof(float), cudaMemcpyDeviceToHost));
    return m_count > 0 ? total / float(m_count) : 0.0f;
}

const std::vector<float2>& SimGpu::positions() const
{
    return m_positions;
}

void SimGpu::readState(Particles& state)
{
    const size_t bytes = m_count * sizeof(float2);
    state.pos.resize(m_count);
    state.vel.resize(m_count);
    CUDA_CHECK(cudaMemcpy(state.pos.data(), m_device.d_pos, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(state.vel.data(), m_device.d_vel, bytes, cudaMemcpyDeviceToHost));
}

void SimGpu::shutdown()
{
    if (m_count == 0)
    {
        return;
    }

    CUDA_CHECK(cudaFree(m_device.d_pos));
    CUDA_CHECK(cudaFree(m_device.d_vel));
    CUDA_CHECK(cudaFree(m_deviceOut.d_pos));
    CUDA_CHECK(cudaFree(m_deviceOut.d_vel));
    CUDA_CHECK(cudaFree(d_partner));
    CUDA_CHECK(cudaFree(d_blockSums));
    CUDA_CHECK(cudaFree(d_total));
    d_partner = nullptr;
    d_blockSums = d_total = nullptr;
    m_grid.shutdown();
    m_cellIds.clear();
    m_hasCellIds = false;
    m_device = DeviceParticles();
    m_deviceOut = DeviceParticles();
    m_positions.clear();

    m_count = 0;
}
