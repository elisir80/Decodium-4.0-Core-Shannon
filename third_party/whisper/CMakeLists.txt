# whisper.cpp dentro Decodium — solo la libreria, solo la CPU.
#
# Non e' il CMakeLists del progetto originale: quello costruisce anche esempi,
# prove e binding per altri linguaggi, e si ferma cercando un file dei binding
# JavaScript che qui non c'e'. Serve una cosa sola — libwhisper.a — e questo
# file la produce.
#
# I backend per GPU (CUDA, Vulkan, Metal, SYCL, OpenCL, Hexagon) non sono stati
# nemmeno copiati: erano diciassette megabyte di sorgenti per acceleratori che
# su una stazione radio non ci sono, e il riconoscimento gira comodo sulla CPU
# — sui trentadue core di questa macchina il modello small sta ben dentro il
# tempo reale.
#
# Aggiornare: ricopiare src/, include/, ggml/{include,cmake,src/*.c*,ggml-cpu}
# da https://github.com/ggml-org/whisper.cpp e lasciare questo file com'e'.
# Licenza MIT, in LICENSE qui accanto.

cmake_minimum_required (VERSION 3.16)

# ── ggml, il motore di calcolo ──────────────────────────────────────────────
# GGML_NATIVE spento di proposito: acceso, ggml compila per le istruzioni della
# macchina che costruisce, e il binario si rifiuta di partire su un PC piu'
# vecchio. Un installer che va solo sul computer di chi l'ha fatto non e' un
# installer. Restano attive AVX2 e FMA, presenti su tutto cio' che ha meno di
# dieci anni.
set (GGML_NATIVE   OFF CACHE BOOL "" FORCE)
set (GGML_CCACHE   OFF CACHE BOOL "" FORCE)
set (GGML_OPENMP   OFF CACHE BOOL "" FORCE)   # il pool OpenMP e' gia' quello dei decodificatori
set (GGML_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set (GGML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set (BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)

add_subdirectory (ggml)

# ── whisper ─────────────────────────────────────────────────────────────────
add_library (whisper STATIC
    src/whisper.cpp
    src/whisper-arch.h
    include/whisper.h
)
target_include_directories (whisper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries (whisper PUBLIC ggml)
target_compile_features (whisper PRIVATE cxx_std_17)

# La versione della libreria, che whisper.cpp scrive nel proprio registro
# all'avvio. Il CMakeLists originale la componeva da tre variabili; qui e'
# scritta una volta e va aggiornata quando si risincronizza.
target_compile_definitions (whisper PRIVATE WHISPER_VERSION="1.9.3")

# Gli avvisi di questo sottoprogetto non sono affar nostro: il codice e' di
# altri e va risincronizzato tale e quale. Fatali qui vorrebbe dire non poterlo
# piu' aggiornare senza rimetterci le mani.
if (MSVC)
  target_compile_options (whisper PRIVATE /W0)
else ()
  target_compile_options (whisper PRIVATE -w)
endif ()
