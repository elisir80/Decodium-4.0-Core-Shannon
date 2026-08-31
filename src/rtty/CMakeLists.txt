# DecoRTTY innestato in Decodium 4.0 — passo 1: le librerie.
#
# Deriva dal CMakeLists di DecoRTTY 0.7.2, ridotto alle sole librerie statiche:
# gli eseguibili del progetto originale (l'applicazione, il gateway FT-991A e
# gli strumenti di prova) non servono qui, perche' il codice viene collegato
# dentro Decodium.
#
# Il gateway FT-991A resta pero' un programma a se' anche nel progetto
# originale, e per una ragione che vale ancora: gira sul PC a cui la radio e'
# collegata, che non e' per forza quello che decodifica. Qui se ne costruisce
# la libreria, e se in futuro servira' l'eseguibile lo si aggiunge senza
# toccare altro.

# ── DSP: demodulatore FSK, Baudot, Viterbi ──────────────────────────────────
# Nessun oggetto Qt: AUTOMOC spento come nell'originale.
add_library (decortty_dsp STATIC
    dsp/Fft.cpp                 dsp/Fft.h
    dsp/Baudot.cpp              dsp/Baudot.h
    dsp/RttyTypes.h
    dsp/RttyDemodulator.cpp     dsp/RttyDemodulator.h
    dsp/ViterbiBaudot.cpp       dsp/ViterbiBaudot.h
    dsp/FirDecimator.cpp        dsp/FirDecimator.h
    dsp/FskModulator.cpp        dsp/FskModulator.h
    dsp/RxFilters.cpp           dsp/RxFilters.h
    dsp/AutoTuner.cpp           dsp/AutoTuner.h
)
target_include_directories (decortty_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties (decortty_dsp PROPERTIES AUTOMOC OFF)
if (MSVC)
  target_compile_options (decortty_dsp PRIVATE /W4)
else ()
  target_compile_options (decortty_dsp PRIVATE -Wall -Wextra -ffast-math)
endif ()

# ── Audio del codec ─────────────────────────────────────────────────────────
# Aprire un dispositivo e portare i campioni a 24 kHz: lo stesso mestiere che
# serve al gateway e all'applicazione quando l'audio arriva da una scheda
# invece che dalla rete.
add_library (decortty_audio STATIC
    gateway/CodecAudio.cpp      gateway/CodecAudio.h
)
target_link_libraries (decortty_audio PUBLIC Qt6::Core Qt6::Multimedia decortty_dsp)

# ── Trasporto FlexRadio: VITA-49, API SmartSDR, sorgenti radio ──────────────
add_library (decortty_flex STATIC
    flex/FlexTrace.cpp          flex/FlexTrace.h
    flex/VitaPacket.cpp         flex/VitaPacket.h
    flex/FlexDiscovery.cpp      flex/FlexDiscovery.h
    flex/FlexApiClient.cpp      flex/FlexApiClient.h
    flex/FlexVitaStream.cpp     flex/FlexVitaStream.h
    flex/OpusCodec.cpp          flex/OpusCodec.h
    flex/FlexRadioLink.cpp      flex/FlexRadioLink.h
    vita/VitaContext.cpp        vita/VitaContext.h
    link/RadioLink.h
    link/GatewayLink.cpp        link/GatewayLink.h
    link/RadioHub.cpp           link/RadioHub.h
    link/SoundCardLink.cpp      link/SoundCardLink.h
)
target_link_libraries (decortty_flex PUBLIC
    Qt6::Core Qt6::Network decortty_dsp decortty_audio)

# Opus serve alla radio per l'audio che il client trasmette; senza, DecoRTTY
# riceve lo stesso. La dipendenza resta quindi opzionale e viene riportata al
# momento della configurazione, come nel progetto originale.
if (DECORTTY_OPUS_TARGET)
  target_link_libraries (decortty_flex PUBLIC ${DECORTTY_OPUS_TARGET})
  target_compile_definitions (decortty_flex PUBLIC DECORTTY_HAVE_OPUS=1)
endif ()

# ── Gateway FT-991A: CAT e audio verso la rete ──────────────────────────────
add_library (decortty_gateway STATIC
    gateway/Ft991Cat.cpp        gateway/Ft991Cat.h
    gateway/Ft991Gateway.cpp    gateway/Ft991Gateway.h
)
target_link_libraries (decortty_gateway PUBLIC
    Qt6::Core Qt6::Network Qt6::Multimedia Qt6::SerialPort
    decortty_audio decortty_flex)
target_compile_definitions (decortty_gateway PUBLIC
    DECORTTY_VERSION="${DECORTTY_VERSION}")

# ── Motore e modelli dell'applicazione ──────────────────────────────────────
# Il waterfall e' un QQuickItem: qui serve perche' RttyEngine lo referenzia.
# L'aggancio all'interfaccia di Decodium e' il passo 4 del piano.
add_library (decortty_app STATIC
    app/RttyEngine.cpp          app/RttyEngine.h
    app/ReceiveTextModel.cpp    app/ReceiveTextModel.h
    app/WaterfallItem.cpp       app/WaterfallItem.h
    app/MacroModel.cpp          app/MacroModel.h
    app/QsoLog.cpp              app/QsoLog.h
    app/BandPlan.cpp            app/BandPlan.h
    app/GatewaySupervisor.cpp   app/GatewaySupervisor.h
    app/Language.cpp            app/Language.h
)
target_link_libraries (decortty_app PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia Qt6::SerialPort
    decortty_dsp decortty_flex decortty_gateway)

# I warning restano fatali anche qui, come nel resto di Decodium.
# La tentazione era di allentarli, visto che questo sottoprogetto va
# risincronizzato con il repository DecoRTTY: il primo che si e' presentato
# pero' — 'unused variable c' in ViterbiBaudot.cpp — non era stile ma un
# difetto vero. L'indice del ciclo sui 32 codici Baudot faceva ombra alla
# certezza del frame, e step.certainty finiva per contenere il numero del
# codice invece della certezza, rendendo priva di senso la sfumatura con cui
# l'interfaccia mostra quanto un carattere sia sicuro. Corretto a monte, in
# DecoRTTY, cosi' la prossima sincronizzazione non lo riporta indietro.

# DecoRTTY e' scritto in C++20 (usa std::numbers), Decodium compila a C++17.
# Lo standard si alza sui soli target di questo sottoprogetto: il resto di
# Decodium resta a 17, e nessun altro bersaglio cambia comportamento.
foreach (bersaglio decortty_dsp decortty_audio decortty_flex
                   decortty_gateway decortty_app)
  set_target_properties (${bersaglio} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)
endforeach ()
