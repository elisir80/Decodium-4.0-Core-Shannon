#pragma once

#include <QString>

namespace decodium {
// Classify the scheduled operation, not the first word of its payload.
// TX6 can contain a special-event call such as TEST instead of CQ/QRZ.
inline bool isAutoCqCall(bool enabled, int txNumber, bool calling,
                         bool partnerActive, const QString& payload)
{
    return enabled && txNumber == 6 && calling && !partnerActive
        && !payload.trimmed().isEmpty();
}
}
