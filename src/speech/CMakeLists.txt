# La trascrizione della fonia.
#
# In SSB non c'e' niente da decodificare: c'e' una voce. Questa libreria la
# trascrive e passa il testo per il ricompositore radiantistico, che rimette a
# posto gergo e nominativi — senza quel passaggio un riconoscitore generico
# scrive "Chico" per CQ e nove parole comuni al posto di un nominativo
# compitato.
#
# Il ricompositore sta a se' e non dipende da whisper: e' testo che entra e
# testo che esce, e come tale si prova senza audio, senza modello e senza
# radio. E' il pezzo con piu' valore e il piu' facile da verificare.
add_library (decodium_speech STATIC
    RadioTranscript.cpp     RadioTranscript.h
    SpeechRecognizer.cpp    SpeechRecognizer.h
    SpeechModelStore.cpp    SpeechModelStore.h
)
target_include_directories (decodium_speech PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries (decodium_speech PUBLIC Qt6::Core Qt6::Network whisper)
set_target_properties (decodium_speech PROPERTIES AUTOMOC ON)
