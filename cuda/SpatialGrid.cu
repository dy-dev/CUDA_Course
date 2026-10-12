#include "SpatialGrid.h"

#include "CudaCheck.h"
#include "Params.h"
#include "Physics.h"

#include <thrust/execution_policy.h>
#include <thrust/sort.h>

#include <utility>

namespace
{
    int gridFor(int n, int blockSize)
    {
        return (n + blockSize - 1) / blockSize;
    }
}

// Coordonnée de cellule d'une position, bornée à la grille : une particule
// sortie du monde (murs désactivés) reste rangée dans une cellule du bord.
__device__ int cellCoord(float v, int cells)
{
    const int c = int(floorf(v / kCellSize));
    return min(max(c, 0), cells - 1);
}

__global__ void computeCellIdsKernel(const float2* d_pos, int* d_cellId, int* d_order, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    const float2 p = d_pos[i];
    const int cx = cellCoord(p.x, kGridWidth);
    const int cy = cellCoord(p.y, kGridHeight);
    d_cellId[i] = cy * kGridWidth + cx;
    d_order[i] = i;}

// Déménagement : la particule rangée en position i après le tri est la
// particule d'origine d_order[i]. Tous ses attributs suivent.
__global__ void reorderKernel(const int* d_order, const float2* d_pos, const float2* d_vel,
                              float2* d_posSorted, float2* d_velSorted, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    const int source = d_order[i];
    d_posSorted[i] = d_pos[source];
    d_velSorted[i] = d_vel[source];}

// Après le tri, les particules d'une même cellule sont contiguës : la plage
// d'une cellule commence là où le numéro change et finit avant le suivant.
__global__ void findCellRangesKernel(const int* d_cellId, int* d_cellStart, int* d_cellEnd, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    const int cell = d_cellId[i];
    if (i == 0 || d_cellId[i - 1] != cell)
    {
        d_cellStart[cell] = i;
    }
    if (i == n - 1 || d_cellId[i + 1] != cell)
    {
        d_cellEnd[cell] = i + 1;
    }
}

__global__ void collideGridKernel(const float2* d_pos, const float2* d_vel,
                                  const int* d_cellStart, const int* d_cellEnd,
                                  float2* d_posOut, int* d_partner, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    float2 p = d_pos[i];
    const float2 v = d_vel[i];
    float2 dp = { 0.0f, 0.0f };
    int partner = -1;
    float partnerDist = 2.0f * kRadius;

    const int cx = cellCoord(p.x, kGridWidth);
    const int cy = cellCoord(p.y, kGridHeight);
    for (int ny = max(cy - 1, 0); ny <= min(cy + 1, kGridHeight - 1); ++ny)
    {
        for (int nx = max(cx - 1, 0); nx <= min(cx + 1, kGridWidth - 1); ++nx)
        {
            const int cell = ny * kGridWidth + nx;
            const int start = d_cellStart[cell];
            if (start < 0)
            {
                continue;
            }
            const int end = d_cellEnd[cell];
            for (int j = start; j < end; ++j)
            {
                if (j != i)
                {
                    accumulateContact(p, v, d_pos[j], d_vel[j], j, dp, partner, partnerDist);
                }
            }
        }
    }
    p.x += dp.x;
    p.y += dp.y;
    d_posOut[i] = p;
    d_partner[i] = partner;
}

// Un bloc par cellule. Les particules de la cellule sont traitées par paquets
// de blockDim.x (un thread chacune). Pour chaque cellule voisine, le bloc charge
// ses particules en mémoire partagée par tuiles de blockDim.x : chaque thread en
// charge une, puis tous comparent leur particule à la tuile entière.
__global__ void collideTiledKernel(const float2* d_pos, const float2* d_vel,
                                   const int* d_cellStart, const int* d_cellEnd,
                                   float2* d_posOut, int* d_partner)
{
    __shared__ float2 s_pos[kTileSize];
    __shared__ float2 s_vel[kTileSize];

    // Cellule vide : tout le bloc s'arrête ensemble.
    const int cell = blockIdx.x;
    const int start = d_cellStart[cell];
    if (start < 0)
    {
        return;
    }
    const int end = d_cellEnd[cell];
    const int cx = cell % kGridWidth;
    const int cy = cell / kGridWidth;

    for (int base = start; base < end; base += blockDim.x)
    {
        // Garde de bornes : un thread sans particule ne quitte pas le kernel, il
        // participera aux chargements et aux barrières sans rien calculer.
        const int i = base + threadIdx.x;
        const bool active = i < end;
        const float2 p = active ? d_pos[i] : make_float2(0.0f, 0.0f);
        const float2 v = active ? d_vel[i] : make_float2(0.0f, 0.0f);
        float2 dp = { 0.0f, 0.0f };
        int partner = -1;
        float partnerDist = 2.0f * kRadius;

        for (int ny = max(cy - 1, 0); ny <= min(cy + 1, kGridHeight - 1); ++ny)
        {
            for (int nx = max(cx - 1, 0); nx <= min(cx + 1, kGridWidth - 1); ++nx)
            {
                const int neighbor = ny * kGridWidth + nx;
                const int neighborStart = d_cellStart[neighbor];
                if (neighborStart < 0)
                {
                    continue;
                }
                const int neighborEnd = d_cellEnd[neighbor];

                // ___TODO___ : parcourir les particules de la cellule voisine par tuiles de blockDim.x : chaque thread charge une particule de la tuile (s'il en reste) dans s_pos et s_vel, barrière, puis les threads actifs appellent accumulateContact pour chaque particule de la tuile autre que la leur, barrière avant la tuile suivante
            }
        }

        if (active)
        {
            d_posOut[i] = make_float2(p.x + dp.x, p.y + dp.y);
            d_partner[i] = partner;
        }
    }
}

void SpatialGrid::init(int count)
{
    CUDA_CHECK(cudaMalloc(&d_cellId, count * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_order, count * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_cellStart, kCellCount * sizeof(int)));
    CUDA_CHECK(cudaMalloc(&d_cellEnd, kCellCount * sizeof(int)));

    // État de départ cohérent : toutes les particules dans la cellule 0, toutes les cellules vides.
    CUDA_CHECK(cudaMemset(d_cellId, 0, count * sizeof(int)));
    CUDA_CHECK(cudaMemset(d_cellStart, 0xFF, kCellCount * sizeof(int)));
    CUDA_CHECK(cudaMemset(d_cellEnd, 0, kCellCount * sizeof(int)));
}

void SpatialGrid::shutdown()
{
    CUDA_CHECK(cudaFree(d_cellId));
    CUDA_CHECK(cudaFree(d_order));
    CUDA_CHECK(cudaFree(d_cellStart));
    CUDA_CHECK(cudaFree(d_cellEnd));
    d_cellId = d_order = d_cellStart = d_cellEnd = nullptr;
}

void SpatialGrid::build(DeviceParticles& particles, DeviceParticles& scratch, int count)
{
    const int gridSize = gridFor(count, kBlockSize);

    computeCellIdsKernel<<<gridSize, kBlockSize>>>(particles.d_pos, d_cellId, d_order, count);
    CUDA_CHECK_LAUNCH();

    // Tri par clé : les numéros de cellule sont triés, les étiquettes suivent.
    thrust::sort_by_key(thrust::device, d_cellId, d_cellId + count, d_order);

    reorderKernel<<<gridSize, kBlockSize>>>(d_order, particles.d_pos, particles.d_vel, scratch.d_pos, scratch.d_vel, count);
    CUDA_CHECK_LAUNCH();
    std::swap(particles, scratch);

    // Toutes les cellules vides (octets 0xFF : -1), puis plages des cellules occupées.
    CUDA_CHECK(cudaMemset(d_cellStart, 0xFF, kCellCount * sizeof(int)));
    findCellRangesKernel<<<gridSize, kBlockSize>>>(d_cellId, d_cellStart, d_cellEnd, count);
    CUDA_CHECK_LAUNCH();
}

void SpatialGrid::collideTiled(const DeviceParticles& particles, float2* d_posOut, int* d_partner)
{
    collideTiledKernel<<<kCellCount, kTileSize>>>(
        particles.d_pos, particles.d_vel, d_cellStart, d_cellEnd, d_posOut, d_partner);
    CUDA_CHECK_LAUNCH();
}

void SpatialGrid::collide(const DeviceParticles& particles, float2* d_posOut, int* d_partner, int count)
{
    collideGridKernel<<<gridFor(count, kBlockSize), kBlockSize>>>(
        particles.d_pos, particles.d_vel, d_cellStart, d_cellEnd, d_posOut, d_partner, count);
    CUDA_CHECK_LAUNCH();
}
