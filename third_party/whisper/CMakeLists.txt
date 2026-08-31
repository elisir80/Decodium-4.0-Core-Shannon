# whisper.cpp dentro Decodium — solo la libreria, con backend portabili.
#
# Non e' il CMakeLists del progetto originale: quello costruisce anche esempi,
# prove e binding per altri linguaggi, e si ferma cercando un file dei binding
# JavaScript che qui non c'e'. Serve una cosa sola — libwhisper.a — e questo
# file la produce.
#
# La CPU resta disponibile su tutte le piattaforme. Su macOS ggml abilita inoltre
# Accelerate/BLAS e Metal per impostazione predefinita; i relativi sorgenti fanno
# parte del vendor import per evitare configurazioni Speech incomplete.
#
# Aggiornare: ricopiare src/, include/ e i componenti ggml usati (inclusi
# ggml-cpu, ggml-blas e ggml-metal) dallo stesso tag di
# https://github.com/ggml-org/whisper.cpp, quindi aggiornare WHISPER_VERSION.
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
set (GGML_ALL_WARNINGS   OFF CACHE BOOL "" FORCE)
set (GGML_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set (BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)

add_subdirectory (ggml)

# Decodium compila il proprio codice con gli avvisi fatali. Non propagare tale
# politica ai sorgenti ggml di terze parti: in particolare gli header recenti
# di Accelerate annotano alcune API BLAS come macOS 13.3 anche se il deployment
# target di Decodium resta macOS 13.0.
foreach (ggml_target IN ITEMS ggml-base ggml ggml-cpu ggml-blas ggml-metal)
  if (TARGET ${ggml_target})
    if (MSVC)
      target_compile_options (${ggml_target} PRIVATE /W0)
    else ()
      target_compile_options (${ggml_target} PRIVATE -w)
    endif ()
  endif ()
endforeach ()

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
