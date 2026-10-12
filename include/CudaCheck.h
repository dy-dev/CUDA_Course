#pragma once

#include <cuda_runtime.h>

#include <cstdio>
#include <cstdlib>

// Vérifie le code de retour d'un appel à l'API CUDA ; en cas d'échec, indique
// où et pourquoi, puis quitte.
#define CUDA_CHECK(call)                                                       \
    do                                                                         \
    {                                                                          \
        const cudaError_t cudaCheckError = (call);                             \
        if (cudaCheckError != cudaSuccess)                                     \
        {                                                                      \
            std::fprintf(stderr, "Erreur CUDA, %s ligne %d : %s\n",            \
                         __FILE__, __LINE__, cudaGetErrorString(cudaCheckError)); \
            std::exit(EXIT_FAILURE);                                           \
        }                                                                      \
    } while (0)

// Vérifie le dernier lancement de kernel. Un lancement ne renvoie rien : une
// configuration refusée se lit avec cudaGetLastError. Une erreur pendant
// l'exécution n'apparaît qu'à la synchronisation suivante ; en Debug, on
// synchronise tout de suite pour la rattacher au bon kernel.
#if defined(NDEBUG)
#define CUDA_CHECK_LAUNCH() CUDA_CHECK(cudaGetLastError())
#else
#define CUDA_CHECK_LAUNCH()                                                    \
    do                                                                         \
    {                                                                          \
        CUDA_CHECK(cudaGetLastError());                                        \
        CUDA_CHECK(cudaDeviceSynchronize());                                   \
    } while (0)
#endif
