#pragma once

#include <QString>
#include <QVariantMap>

namespace decodium {
namespace decode_ui {

inline bool isWorkedCq(QVariantMap const& entry)
{
    if (!entry.value(QStringLiteral("isCQ")).toBool()) {
        return false;
    }

    return entry.value(QStringLiteral("isB4")).toBool()
        || entry.value(QStringLiteral("dxIsWorked")).toBool()
        || entry.value(QStringLiteral("dxIsWorkedBand")).toBool();
}

inline bool isHiddenByWorkedFilters(QVariantMap const& entry,
                                    bool hideWorkedBand,
                                    bool hideWorkedToday,
                                    bool preserveWorkedCq)
{
    // "CQ Only" is a presentation mode: a CQ must remain available to the
    // delegate so that the B4 colour and optional strikeout can be rendered.
    // Explicit worked-station hiding keeps its usual behaviour in other modes.
    if (preserveWorkedCq && isWorkedCq(entry)) {
        return false;
    }

    return (hideWorkedBand
            && entry.value(QStringLiteral("dxIsWorkedBand")).toBool())
        || (hideWorkedToday
            && entry.value(QStringLiteral("dxIsWorkedToday")).toBool());
}

} // namespace decode_ui
} // namespace decodium
