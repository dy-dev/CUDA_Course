#pragma once

#include <cuda_runtime.h>

// Vérifie le code de retour d'un appel à l'API CUDA.
// ___TODO___ : si l'appel échoue, afficher le fichier, la ligne et le message de cudaGetErrorString, puis quitter le programme
#define CUDA_CHECK(call) call

// Vérifie le dernier lancement de kernel.
// ___TODO___ : vérifier l'erreur de lancement ; en Debug seulement, attendre aussi la fin du kernel et vérifier son exécution
#define CUDA_CHECK_LAUNCH()
