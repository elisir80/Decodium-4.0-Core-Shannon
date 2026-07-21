#ifndef DECODIUM_QSO_SEQUENCER_RULES_HPP
#define DECODIUM_QSO_SEQUENCER_RULES_HPP

// Fase 1 port mobile — step A dello strangler (doc/mobile-port-plan.md):
// regole PURE del sequencer QSO estratte da DecodiumBridge.cpp, condivise
// tra desktop e decodium-core mobile. Nessuno stato, nessun side effect:
// ogni funzione dipende solo dai parametri.

#include <QString>

namespace decodium
{
namespace seq
{

// Cap ripetizioni 73/RR73 per modo (FT2/FT4/FT8: valore utente assoluto;
// altri modi: legacy modeCap + extra QSB/conservative). Estratta verbatim
// da DecodiumBridge.cpp (era static, 1.0.311/315/437).
int deferredSignoffRetryCapForMode (const QString& mode, int configuredMaxRetries,
                                    int partnerSnrDb = 127, bool conservative = false,
                                    bool quickGiveUpStrong = false,
                                    int ft2SignoffCap = 8,
                                    int ft4SignoffCap = 4,
                                    int ft8SignoffCap = 3, bool weakBoost = false,
                                    int weakSnrThreshold = -15, int weakBonus = 3);

}
}

#endif
