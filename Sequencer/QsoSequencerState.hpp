#ifndef DECODIUM_QSO_SEQUENCER_STATE_HPP
#define DECODIUM_QSO_SEQUENCER_STATE_HPP

// Fase 1 port mobile — step B strangler (doc/mobile-port-plan.md): lo stato
// mutabile del sequencer QSO, raggruppato in un POD portabile che desktop e
// decodium-core mobile condividono. Modello: MamQsoSlot (DecodiumBridge.h), che
// già replica lo stato mono-QSO in un POD. In DecodiumBridge i vecchi membri
// m_* diventano riferimenti-alias a questi campi (stessi nomi/default) → zero
// modifiche ai call-site; allo step D il QsoSequencer estratto prenderà
// QsoSequencerState& e i riferimenti-alias spariranno.

#include <QHash>
#include <QString>
#include <QtGlobal>

namespace decodium
{
namespace seq
{

struct QsoSequencerState
{
    // --- Blocco "TxController clone" (1° increment step B) ---
    int  nTx73         {0};    // 73/RR73 completati nel QSO corrente
    int  txRetryCount  {0};    // invii di lastNtx senza risposta
    int  lastNtx       {-1};   // ultimo TX number inviato
    int  lastCqPidx    {-1};   // period index dell'ultimo CQ (evita CQ consecutivi)
    QString lastAutoSeqKey;    // deduplicazione autoSequenceStep
    qint64  lastAutoSeqMs {0}; // timestamp ultima deduplicazione
    QHash<QString, qint64>  recentDirectedReportDecodeMs;
    QHash<QString, QString> recentDirectedReportDecodeMessage;
    QString lastTransmittedMessage;
    QString autoSeqRogerReportBase;
    int     activeTxNumber {0};
    QString activeTxMessage;
};

}
}

#endif
