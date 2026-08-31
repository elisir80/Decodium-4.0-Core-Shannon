// Il punto in cui DecoRTTY entra in Decodium.
//
// Passo 2 del piano (doc/PIANO_INTEGRAZIONE_DECORTTY.md), strada (a): DecoRTTY
// tiene la propria sorgente audio. Il flusso VITA-49 arriva dal RadioHub
// direttamente al motore RTTY senza passare dal percorso audio di Decodium,
// che resta intatto — e con esso FT8, FT4 e FT2. La strada (b), FlexRadio come
// sorgente per tutti i modi, e' un progetto a se' da affrontare dopo e con il
// suo banco di prova: quel percorso e' lo stesso che ha gia' lasciato la
// ricezione ferma quando una sorgente remota e' rimasta attiva a monitor spento.
//
// Qui si mettono insieme gli oggetti che nel progetto originale vivevano in
// main.cpp, cosi' il resto di Decodium ne vede uno solo. La proprieta' e' di
// questa classe: nessuno di essi sopravvive all'host.

#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>

#include "app/GatewaySupervisor.h"
#include "app/Language.h"
#include "app/MacroModel.h"
#include "app/QsoLog.h"
#include "app/ReceiveTextModel.h"
#include "app/RttyEngine.h"
#include "link/RadioHub.h"

class QQmlContext;
class QSettings;

namespace decortty {

class DecoRttyHost : public QObject
{
    Q_OBJECT

    // Vero quando il motore RTTY sta ricevendo: serve a Decodium per sapere se
    // il sottosistema e' vivo senza doverne conoscere i dettagli.
    Q_PROPERTY (bool attivo READ attivo NOTIFY attivoChanged)

public:
    explicit DecoRttyHost (QObject* parent = nullptr);
    ~DecoRttyHost () override;

    // Carica le impostazioni salvate e collega gli oggetti fra loro. Va
    // chiamata una volta, dopo la costruzione.
    void avvia (QSettings& impostazioni);

    // Espone gli oggetti al QML con gli stessi nomi del progetto originale, in
    // modo che i file .qml di DecoRTTY funzionino senza modifiche.
    void esponiAlQml (QQmlContext& contesto, QString const& versione);

    bool attivo () const { return m_attivo; }

    link::RadioHub&           radio        () { return m_radio; }
    app::RttyEngine&          motore       () { return m_motore; }
    app::ReceiveTextModel&    testoRicevuto() { return m_testoRicevuto; }
    app::MacroModel&          macro        () { return m_macro; }
    app::QsoLog&              logQso       () { return m_logQso; }
    app::GatewaySupervisor&   gateway      () { return m_gateway; }
    app::Language&            lingua       () { return m_lingua; }

signals:
    void attivoChanged ();

    // Una riga di testo completa, pronta per la lista dei decodificati di
    // Decodium. RTTY e' un flusso continuo: il testo scorre nella finestra
    // dedicata carattere per carattere, mentre qui esce solo quando la riga e'
    // chiusa — da un ritorno a capo o da una pausa nel segnale. Senza questo
    // taglio la lista si riempirebbe di frammenti.
    void rigaDecodificata (QString const& testo, double qualita, double frequenzaHz);

    // L'audio ricevuto dalla radio, a 24 kHz, per il waterfall di Decodium.
    // Si passa l'audio e non uno spettro gia' calcolato: il waterfall resta
    // quello dell'applicazione, con la sua resa e i suoi comandi.
    void audioPerWaterfall (QVector<float> const& campioni24k);

private:
    void collegaTestoRicevuto ();
    void accumulaCarattere (QString const& carattere, double qualita);
    void chiudiRiga ();

    QString m_rigaInCorso;
    double  m_qualitaSomma {0.0};
    int     m_qualitaConteggio {0};
    QTimer* m_pausaRiga {nullptr};

    app::Language          m_lingua;
    link::RadioHub         m_radio;
    app::RttyEngine        m_motore;
    app::GatewaySupervisor m_gateway;
    app::ReceiveTextModel  m_testoRicevuto;
    app::MacroModel        m_macro;
    app::QsoLog            m_logQso;
    bool                   m_attivo {false};
};

} // namespace decortty
