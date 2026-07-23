#ifndef DECODIUM_QSO_SEQUENCER_HPP
#define DECODIUM_QSO_SEQUENCER_HPP

// Fase 1 port mobile — step C strangler: scheletro della classe che, allo step
// D, ospiterà i corpi della logica di sequencing oggi in DecodiumBridge
// (checkAndStartPeriodicTx, autoSequenceStep, advanceQsoState, …). Opera SOLO
// su QsoSequencerState (lo stato, già raggruppato negli step B/B2/B3) e
// ISequencerSink (il seam, step C). Nessuna dipendenza da Qt-GUI, QSettings,
// audio o CAT → compilabile identico su desktop e mobile.
//
// Migrazione (step D): ogni funzione si sposta qui una alla volta; DecodiumBridge
// delega chiamando m_sequencer.<funzione>(), passando *this come sink. Gate per
// funzione: loopback ALPHA+BRAVO + diff del diagnostic log.

#include "Sequencer/ISequencerSink.hpp"
#include "Sequencer/QsoSequencerState.hpp"

namespace decodium
{
namespace seq
{

class QsoSequencer
{
public:
    QsoSequencer(QsoSequencerState& state, ISequencerSink& sink)
        : m_state(state), m_sink(sink)
    {
    }

    // Non copiabile (tiene riferimenti).
    QsoSequencer(const QsoSequencer&) = delete;
    QsoSequencer& operator=(const QsoSequencer&) = delete;

    QsoSequencerState& state() { return m_state; }
    ISequencerSink&    sink()  { return m_sink; }

    // I metodi di logica (advanceQsoState, checkAndStartPeriodicTx, …) arriveranno
    // allo step D, spostati verbatim da DecodiumBridge e ricablati su m_state/m_sink.

private:
    QsoSequencerState& m_state;
    ISequencerSink&    m_sink;
};

}
}

#endif
