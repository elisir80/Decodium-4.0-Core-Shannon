#include "DecoRttyHost.h"

#include <QQmlContext>
#include <QSettings>

namespace decortty {

DecoRttyHost::DecoRttyHost (QObject* parent)
    : QObject {parent}
{
}

DecoRttyHost::~DecoRttyHost () = default;

void DecoRttyHost::collegaTestoRicevuto ()
{
    // Caratteri decodificati e trasmessi finiscono nella stessa finestra, cosi'
    // l'operatore legge il QSO come una conversazione. E' il comportamento del
    // progetto originale e va mantenuto.
    connect (&m_motore, &app::RttyEngine::characterDecoded, &m_testoRicevuto,
             [this] (QString const& testo, double qualita, bool corretto) {
                 if (!testo.isEmpty ())
                     m_testoRicevuto.appendCharacter (testo.at (0), qualita, corretto);
             });
    connect (&m_motore, &app::RttyEngine::characterTransmitted, &m_testoRicevuto,
             [this] (QString const& testo) {
                 if (!testo.isEmpty ())
                     m_testoRicevuto.appendTransmitted (testo.at (0));
             });
}

void DecoRttyHost::avvia (QSettings& impostazioni)
{
    m_macro.load (impostazioni);
    m_motore.attachRadio (&m_radio);
    collegaTestoRicevuto ();

    // Impostazioni del decodificatore, con gli stessi valori di partenza del
    // progetto originale: chi passa da DecoRTTY a Decodium ritrova la sua
    // configurazione, perche' le chiavi sono le stesse.
    m_motore.setMarkHz            (impostazioni.value (QStringLiteral ("rtty/markHz"), 2125.0).toDouble ());
    m_motore.setShiftHz           (impostazioni.value (QStringLiteral ("rtty/shiftHz"), 170.0).toDouble ());
    m_motore.setBaud              (impostazioni.value (QStringLiteral ("rtty/baud"), 45.45).toDouble ());
    m_motore.setReverse           (impostazioni.value (QStringLiteral ("rtty/reverse"), false).toBool ());
    m_motore.setUnshiftOnSpace    (impostazioni.value (QStringLiteral ("rtty/usos"), true).toBool ());
    m_motore.setAfcEnabled        (impostazioni.value (QStringLiteral ("rtty/afc"), true).toBool ());
    m_motore.setAutoTuneEnabled   (impostazioni.value (QStringLiteral ("rtty/autoTune"), true).toBool ());
    m_motore.setSquelchDb         (impostazioni.value (QStringLiteral ("rtty/squelchDb"), 4.0).toDouble ());
    m_motore.setCorrectionDepth   (impostazioni.value (QStringLiteral ("rtty/correctionDepth"), 4).toInt ());
    m_motore.setTransmitLevel     (impostazioni.value (QStringLiteral ("rtty/transmitLevel"), 0.35).toDouble ());
    m_motore.setStopBits          (impostazioni.value (QStringLiteral ("rtty/stopBits"), 1.5).toDouble ());
    m_motore.setDataBits          (impostazioni.value (QStringLiteral ("rtty/dataBits"), 5).toInt ());
    m_motore.setParity            (impostazioni.value (QStringLiteral ("rtty/parity"), 0).toInt ());
    m_motore.setFiguresSet        (impostazioni.value (QStringLiteral ("rtty/figuresSet"), 0).toInt ());
    m_motore.setIgnoreFramingErrors (impostazioni.value (QStringLiteral ("rtty/ignoreFraming"), false).toBool ());
    m_motore.setBandpassEnabled   (impostazioni.value (QStringLiteral ("rtty/bandpass"), false).toBool ());
    m_motore.setBandpassWidthHz   (impostazioni.value (QStringLiteral ("rtty/bandpassWidth"), 500.0).toDouble ());
    m_motore.setLmsEnabled        (impostazioni.value (QStringLiteral ("rtty/lms"), false).toBool ());
    m_motore.setDiddleMode        (impostazioni.value (QStringLiteral ("rtty/diddleMode"), 1).toInt ());
    m_motore.setCharacterWaitBits (impostazioni.value (QStringLiteral ("rtty/charWait"), 0.0).toDouble ());

    // Il gateway FT-991A resta SPENTO per difetto dentro Decodium, al contrario
    // del progetto originale dove si apriva con l'applicazione. Qui la radio e'
    // gia' governata dal CAT di Decodium, e due programmi sulla stessa porta
    // seriale se la contendono: chi vuole mettere la radio in rete lo accende
    // di proposito, dopo aver liberato la porta.
    m_gateway.setEnabled     (impostazioni.value (QStringLiteral ("gateway/enabled"), false).toBool ());
    m_gateway.setCatPort     (impostazioni.value (QStringLiteral ("gateway/catPort"), QStringLiteral ("COM5")).toString ());
    m_gateway.setAudioIn     (impostazioni.value (QStringLiteral ("gateway/audioIn"), QStringLiteral ("USB Audio CODEC")).toString ());
    m_gateway.setAudioOut    (impostazioni.value (QStringLiteral ("gateway/audioOut"), QStringLiteral ("USB Audio CODEC")).toString ());
    m_gateway.setUdpPort     (impostazioni.value (QStringLiteral ("gateway/udpPort"), 4993).toInt ());
    m_gateway.setAutoConnect (impostazioni.value (QStringLiteral ("gateway/autoConnect"), false).toBool ());
    m_gateway.setCallsign    (m_macro.myCall ());

    m_attivo = true;
    emit attivoChanged ();
}

void DecoRttyHost::esponiAlQml (QQmlContext& contesto, QString const& versione)
{
    // Stessi nomi del progetto originale: i file .qml di DecoRTTY funzionano
    // senza modifiche, e le loro correzioni si possono riportare qui cosi'
    // come sono.
    contesto.setContextProperty (QStringLiteral ("radio"),       &m_radio);
    contesto.setContextProperty (QStringLiteral ("rtty"),        &m_motore);
    contesto.setContextProperty (QStringLiteral ("receiveText"), &m_testoRicevuto);
    contesto.setContextProperty (QStringLiteral ("macros"),      &m_macro);
    contesto.setContextProperty (QStringLiteral ("qsoLog"),      &m_logQso);
    contesto.setContextProperty (QStringLiteral ("gateway"),     &m_gateway);
    contesto.setContextProperty (QStringLiteral ("language"),    &m_lingua);
    contesto.setContextProperty (QStringLiteral ("appVersion"),  versione);
}

} // namespace decortty
