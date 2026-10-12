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

    // ___TODO___ : ranger dans d_cellId[i] le numéro de la cellule qui contient la particule (coordonnées de cellule bornées avec cellCoord, puis numérotation ligne par ligne) et dans d_order[i] l'indice de la particule
}

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

    // ___TODO___ : recopier en position i des tableaux triés la position et la vitesse de la particule d'origine d_order[i]
}

// Après le tri, les particules d'une même cellule sont contiguës : la plage
// d'une cellule commence là où le numéro change et finit avant le suivant.
__global__ void findCellRangesKernel(const int* d_cellId, int* d_cellStart, int* d_cellEnd, int n)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n)
    {
        return;
    }

    // ___TODO___ : si la particule i est la première de sa cellule, noter i comme début de la cellule ; si elle en est la dernière, noter i + 1 comme fin
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

    // ___TODO___ : parcourir les 3 × 3 cellules autour de celle de la particule (en restant dans la grille, en sautant les cellules vides) et appeler accumulateContact pour chaque autre particule de leurs plages

    p.x += dp.x;
    p.y += dp.y;
    d_posOut[i] = p;
    d_partner[i] = partner;
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

    // ___TODO___ : trier d_cellId sur la carte en emportant les étiquettes d_order (tri par clé de Thrust)

    reorderKernel<<<gridSize, kBlockSize>>>(d_order, particles.d_pos, particles.d_vel, scratch.d_pos, scratch.d_vel, count);
    CUDA_CHECK_LAUNCH();
    // ___TODO___ : échanger particles et scratch, pour que l'état trié devienne l'état courant

    // ___TODO___ : marquer toutes les cellules comme vides (début à -1) avant de chercher les plages
    findCellRangesKernel<<<gridSize, kBlockSize>>>(d_cellId, d_cellStart, d_cellEnd, count);
    CUDA_CHECK_LAUNCH();
}

void SpatialGrid::collide(const DeviceParticles& particles, float2* d_posOut, int* d_partner, int count)
{
    collideGridKernel<<<gridFor(count, kBlockSize), kBlockSize>>>(
        particles.d_pos, particles.d_vel, d_cellStart, d_cellEnd, d_posOut, d_partner, count);
    CUDA_CHECK_LAUNCH();
}
