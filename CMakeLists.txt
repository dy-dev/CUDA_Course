cmake_minimum_required(VERSION 3.24)

# Deux configurations : Debug pour la mise au point, Release pour les mesures.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

project(CudaParticles LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# 86 : RTX 3070 Ti, 89 : RTX 4080
set(CMAKE_CUDA_ARCHITECTURES "86;89")

# Générateurs à configuration unique (Makefile, Ninja) : Release par défaut.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug ou Release" FORCE)
endif()

find_package(CUDAToolkit REQUIRED)

# ---------------------------------------------------------------------------
# Dépendances : raylib (fenêtre, dessin) et raygui (panneau), versions figées
# ---------------------------------------------------------------------------
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# raylib 5.5 : dernière version 5.x stable
FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# raygui 4.0 : en-tête seul, compatible raylib 5.x. SOURCE_SUBDIR pointe vers un
# dossier absent pour que CMake se contente de télécharger les sources.
FetchContent_Declare(raygui
    URL https://github.com/raysan5/raygui/archive/refs/tags/4.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR no-cmake)

FetchContent_MakeAvailable(raylib raygui)

# Options communes à nos cibles (pas aux dépendances).
function(configure_target target)
    if(MSVC)
        # Sources en UTF-8 (accents des commentaires et des textes affichés).
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
            $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/utf-8>)
    endif()
    # Correspondance source / instructions pour les profileurs (sans coût à l'exécution).
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-lineinfo>)
    if(WIN32)
        # Sous-système console : les messages d'erreur restent visibles.
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(particles
    src/main.cpp
    src/Ui.cpp
)
target_include_directories(particles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${raygui_SOURCE_DIR}/src)
target_link_libraries(particles PRIVATE raylib CUDA::cudart_static)
if(WIN32)
    # raylib s'appuie sur winmm pour la précision du minuteur.
    target_link_libraries(particles PRIVATE winmm)
endif()
configure_target(particles)

# ---------------------------------------------------------------------------
# Mesures console
# ---------------------------------------------------------------------------
add_executable(bench_cpu bench/BenchCpu.cpp)
configure_target(bench_cpu)

add_executable(bench_gpu bench/BenchGpu.cu)
configure_target(bench_gpu)
