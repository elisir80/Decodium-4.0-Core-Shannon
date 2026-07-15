#include "DecodeListModel.h"

#include <utility>

namespace {
// Mapping role → field name nella QVariantMap. Allinea con quello che
// enrichDecodeEntry() di DecodiumBridge popola.
struct RoleSpec {
    int role;
    char const* qmlName;
    char const* mapKey;
};

static RoleSpec const kRoleSpecs[] = {
    { DecodeListModel::TimeRole,                 "time",               "time" },
    { DecodeListModel::UtcRole,                  "utc",                "utc" },
    { DecodeListModel::DbRole,                   "db",                 "db" },
    { DecodeListModel::DtRole,                   "dt",                 "dt" },
    { DecodeListModel::FreqRole,                 "freq",               "freq" },
    { DecodeListModel::MessageRole,              "message",            "message" },
    { DecodeListModel::DisplayMessageRole,       "displayMessage",     "displayMessage" },
    { DecodeListModel::ModeRole,                 "mode",               "mode" },
    { DecodeListModel::TimestampRole,            "timestamp",          "timestamp" },
    { DecodeListModel::IsTxRole,                 "isTx",               "isTx" },
    { DecodeListModel::IsCQRole,                 "isCQ",               "isCQ" },
    { DecodeListModel::IsMyCallRole,             "isMyCall",           "isMyCall" },
    { DecodeListModel::IsB4Role,                 "isB4",               "isB4" },
    { DecodeListModel::IsLotwRole,               "isLotw",             "isLotw" },
    { DecodeListModel::IsSeparatorRole,          "isSeparator",        "isSeparator" },
    { DecodeListModel::MatchesDxCallRole,        "matchesDxCall",      "matchesDxCall" },
    { DecodeListModel::FromCallRole,             "fromCall",           "fromCall" },
    { DecodeListModel::DxCallsignRole,           "dxCallsign",         "dxCallsign" },
    { DecodeListModel::DxCountryRole,            "dxCountry",          "dxCountry" },
    { DecodeListModel::DxContinentRole,          "dxContinent",        "dxContinent" },
    { DecodeListModel::DxPrefixRole,             "dxPrefix",           "dxPrefix" },
    { DecodeListModel::UsStateRole,              "usState",            "usState" },
    { DecodeListModel::DxBearingRole,            "dxBearing",          "dxBearing" },
    { DecodeListModel::DxDistanceRole,           "dxDistance",         "dxDistance" },
    { DecodeListModel::DxIsMostWantedRole,       "dxIsMostWanted",     "dxIsMostWanted" },
    { DecodeListModel::DxIsNewCountryRole,       "dxIsNewCountry",     "dxIsNewCountry" },
    { DecodeListModel::DxIsNewBandRole,          "dxIsNewBand",        "dxIsNewBand" },
    { DecodeListModel::DxIsWorkedRole,           "dxIsWorked",         "dxIsWorked" },
    { DecodeListModel::DxIsNewDxccBandRole,      "dxIsNewDxccBand",    "dxIsNewDxccBand" },
    { DecodeListModel::DxIsNewGridRole,          "dxIsNewGrid",        "dxIsNewGrid" },
    { DecodeListModel::DxIsNewCqZoneRole,        "dxIsNewCqZone",      "dxIsNewCqZone" },
    { DecodeListModel::DxIsNewContinentRole,     "dxIsNewContinent",   "dxIsNewContinent" },
    { DecodeListModel::DxIsNewCallRole,          "dxIsNewCall",        "dxIsNewCall" },
    { DecodeListModel::HighlightBgRole,          "highlightBg",        "highlightBg" },
    { DecodeListModel::IsHighlightedRole,        "isHighlighted",      "isHighlighted" },
    { DecodeListModel::AptypeRole,               "aptype",             "aptype" },
    { DecodeListModel::DriftRole,                "drift",              "drift" },
    { DecodeListModel::ForceRxPaneRole,          "forceRxPane",        "forceRxPane" },
    { DecodeListModel::QualityRole,              "quality",            "quality" },
    // Role speciale che ritorna l'intera entry (per delegate che usano
    // modelData come oggetto-tipo: model.modelData.X). Qt6 espone anche
    // i singoli ruoli, ma "modelData" si bind via questo all'oggetto intero.
    { DecodeListModel::EntryRole,                "modelData",          "" },
};

QString roleFieldKey(int role)
{
    for (auto const& spec : kRoleSpecs) {
        if (spec.role == role) {
            return QString::fromLatin1(spec.mapKey);
        }
    }
    return QString();
}

} // namespace

DecodeListModel::DecodeListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    for (auto const& spec : kRoleSpecs) {
        m_roleNames.insert(spec.role, QByteArray(spec.qmlName));
    }
}

int DecodeListModel::rowCount(QModelIndex const& parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

QVariant DecodeListModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }
    QVariantMap const& entry = m_entries.at(index.row());

    if (role == EntryRole) return entry;

    QString const key = roleFieldKey(role);
    if (key.isEmpty()) return QVariant();
    return entry.value(key);
}

QHash<int, QByteArray> DecodeListModel::roleNames() const
{
    return m_roleNames;
}

QVariantMap DecodeListModel::entry(int index) const
{
    if (index < 0 || index >= m_entries.size()) return QVariantMap();
    return m_entries.at(index);
}

QString DecodeListModel::decodeMatchKey(QVariantMap const& entry)
{
    if (entry.value(QStringLiteral("isSeparator")).toBool()) {
        // 1.0.149: chiave separator stabile = time + timestamp ms. Per FT2
        // async time e' vuoto e tutti i separator avevano stessa key "sep|"
        // -> il diff teneva solo UN separator. Includere il ts li' rende
        // univoci per period FT2.
        QString const t = entry.value(QStringLiteral("time")).toString();
        QString const ts = entry.value(QStringLiteral("timestamp")).toString();
        return QStringLiteral("sep|") + t + QStringLiteral("|") + ts;
    }
    QString const ts = entry.value(QStringLiteral("timestamp")).toString();
    QString const freq = entry.value(QStringLiteral("freq")).toString();
    QString const msg = entry.value(QStringLiteral("message")).toString();
    QString const time = entry.value(QStringLiteral("time")).toString();
    QString const isTx = entry.value(QStringLiteral("isTx")).toBool() ? QStringLiteral("T") : QStringLiteral("R");
    if (!ts.isEmpty()) {
        return isTx + QStringLiteral("|") + ts + QStringLiteral("|") + freq + QStringLiteral("|") + msg;
    }
    return isTx + QStringLiteral("|") + time + QStringLiteral("|") + freq + QStringLiteral("|") + msg;
}

void DecodeListModel::setEntries(QVariantList const& newEntries)
{
    int const newCount = newEntries.size();
    int const oldCount = m_entries.size();

    // QVariant::toMap() and decodeMatchKey() are both allocation-heavy. Cache
    // their results once per snapshot so the structural probes below stay
    // linear even when early/final/deep passes replace a busy 500-row model.
    QVector<QVariantMap> incomingEntries;
    incomingEntries.reserve(newCount);
    QVector<QString> newKeys;
    newKeys.reserve(newCount);
    for (QVariant const& value : newEntries) {
        QVariantMap const entry = value.toMap();
        incomingEntries.append(entry);
        newKeys.append(decodeMatchKey(entry));
    }

    QVector<QString> oldKeys;
    oldKeys.reserve(oldCount);
    for (QVariantMap const& entry : std::as_const(m_entries)) {
        oldKeys.append(decodeMatchKey(entry));
    }

    // 1.0.144: scoped dataChanged — emette dataChanged SOLO per regioni
    // consecutive di row effettivamente cambiate, invece di "tutto il prefix".
    // Su FT2 attivo con append-only, normalmente il prefix è invariato →
    // zero dataChanged emit, solo InsertRows in coda.
    auto applyRangeDiff = [this, &incomingEntries](int modelStart, int newStart, int count) {
        int regionStart = -1;
        for (int i = 0; i < count; ++i) {
            int const modelIndex = modelStart + i;
            QVariantMap const& candidate = incomingEntries.at(newStart + i);
            bool const changed = (m_entries[modelIndex] != candidate);
            if (changed) {
                m_entries[modelIndex] = candidate;
                if (regionStart < 0) regionStart = modelIndex;
            } else if (regionStart >= 0) {
                emit dataChanged(index(regionStart), index(modelIndex - 1));
                regionStart = -1;
            }
        }
        if (regionStart >= 0) {
            emit dataChanged(index(regionStart), index(modelStart + count - 1));
        }
    };

    auto applyPrefixDiff = [&applyRangeDiff](int prefixEnd) {
        applyRangeDiff(0, 0, prefixEnd);
    };

    // --- Caso 1: append-only (prefix identico, append in coda) ---
    if (newCount >= oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < oldCount; ++i) {
            if (oldKeys.at(i) != newKeys.at(i)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            applyPrefixDiff(oldCount);
            if (newCount > oldCount) {
                beginInsertRows(QModelIndex(), oldCount, newCount - 1);
                for (int i = oldCount; i < newCount; ++i) {
                    m_entries.append(incomingEntries.at(i));
                }
                endInsertRows();
            }
            return;
        }
    }

    // --- Caso 2: shrink-only (prefix identico, remove dalla coda) ---
    if (newCount < oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < newCount; ++i) {
            if (oldKeys.at(i) != newKeys.at(i)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            beginRemoveRows(QModelIndex(), newCount, oldCount - 1);
            m_entries.resize(newCount);
            endRemoveRows();
            applyPrefixDiff(newCount);
            return;
        }
    }

    // --- Caso 3 (1.0.207): shift-N-from-head + append-M-to-tail ---
    // Tipico quando cap m_decodeList rimuove oldest e nuovi decode entrano in
    // coda (1.0.206 cap 500). Senza questo caso si cadeva in beginResetModel
    // = ridisegno totale ListView ad ogni decode → Full Spectrum scattoso.
    // Cerco lo shift N tale che oldEntries[N..oldCount-1] match newEntries[0..oldCount-N-1].
    // 1.0.478: busy FT8/FT4/FT2 slots can trim more than 64 old rows at once.
    // Raising the cap avoids beginResetModel() during high decode pile-up.
    if (oldCount > 0 && newCount > 0) {
        int const maxShift = qMin(oldCount, 256);
        for (int shift = 1; shift <= maxShift; ++shift) {
            int const overlapLen = oldCount - shift;
            if (overlapLen <= 0 || overlapLen > newCount) continue;
            bool overlapMatches = true;
            for (int i = 0; i < overlapLen; ++i) {
                if (oldKeys.at(i + shift) != newKeys.at(i)) {
                    overlapMatches = false;
                    break;
                }
            }
            if (!overlapMatches) continue;
            // Match! Applica: rimuovi shift entries dalla testa, poi append M nuove in coda.
            beginRemoveRows(QModelIndex(), 0, shift - 1);
            m_entries.remove(0, shift);
            endRemoveRows();
            int const tailNew = newCount - overlapLen;
            if (tailNew > 0) {
                beginInsertRows(QModelIndex(), overlapLen, overlapLen + tailNew - 1);
                for (int i = overlapLen; i < newCount; ++i) {
                    m_entries.append(incomingEntries.at(i));
                }
                endInsertRows();
            }
            applyPrefixDiff(overlapLen);  // catch in-place value updates su overlap
            return;
        }
    }

    // --- Caso 4: prepend-N + prune-M dalla coda ---
    // Usato dalle viste newest-first e da snapshot che inseriscono il nuovo
    // slot davanti alla history. Prima cadeva nel reset completo del model:
    // Qt Quick distruggeva e ricreava tutti i delegate proprio alla consegna
    // dei risultati FT4/FT8, producendo il blocco grafico periodico.
    if (oldCount > 0 && newCount > 0) {
        int const maxPrepend = qMin(newCount, 256);
        for (int prepend = 1; prepend <= maxPrepend; ++prepend) {
            int const overlapLen = newCount - prepend;
            if (overlapLen <= 0 || overlapLen > oldCount) continue;

            bool overlapMatches = true;
            for (int i = 0; i < overlapLen; ++i) {
                if (oldKeys.at(i) != newKeys.at(prepend + i)) {
                    overlapMatches = false;
                    break;
                }
            }
            if (!overlapMatches) continue;

            if (overlapLen < oldCount) {
                beginRemoveRows(QModelIndex(), overlapLen, oldCount - 1);
                m_entries.resize(overlapLen);
                endRemoveRows();
            }

            beginInsertRows(QModelIndex(), 0, prepend - 1);
            for (int i = prepend - 1; i >= 0; --i) {
                m_entries.insert(0, incomingEntries.at(i));
            }
            endInsertRows();

            applyRangeDiff(prepend, prepend, overlapLen);
            return;
        }
    }

    // --- Caso 5: replace della sola regione realmente cambiata ---
    // Le passate early/final/deep dello stesso slot non sono necessariamente
    // append-only: il decoder puo sostituire il tail provvisorio mantenendo
    // intatta tutta la history. Un model reset distrugge comunque tutti i
    // delegate visibili e coincide con gli scatti FT4/FT8. Conserva invece il
    // prefix e il suffix comuni e sostituisci soltanto la regione centrale.
    int commonPrefix = 0;
    int const commonLimit = qMin(oldCount, newCount);
    while (commonPrefix < commonLimit
           && oldKeys.at(commonPrefix) == newKeys.at(commonPrefix)) {
        ++commonPrefix;
    }

    int commonSuffix = 0;
    while (commonSuffix < oldCount - commonPrefix
           && commonSuffix < newCount - commonPrefix
           && oldKeys.at(oldCount - 1 - commonSuffix)
                == newKeys.at(newCount - 1 - commonSuffix)) {
        ++commonSuffix;
    }

    applyRangeDiff(0, 0, commonPrefix);

    int const oldMiddleCount = oldCount - commonPrefix - commonSuffix;
    if (oldMiddleCount > 0) {
        beginRemoveRows(QModelIndex(), commonPrefix, commonPrefix + oldMiddleCount - 1);
        m_entries.remove(commonPrefix, oldMiddleCount);
        endRemoveRows();
    }

    int const newMiddleCount = newCount - commonPrefix - commonSuffix;
    if (newMiddleCount > 0) {
        beginInsertRows(QModelIndex(), commonPrefix, commonPrefix + newMiddleCount - 1);
        for (int i = 0; i < newMiddleCount; ++i) {
            m_entries.insert(commonPrefix + i, incomingEntries.at(commonPrefix + i));
        }
        endInsertRows();
    }

    if (commonSuffix > 0) {
        int const suffixStart = commonPrefix + newMiddleCount;
        applyRangeDiff(suffixStart, suffixStart, commonSuffix);
    }
}
