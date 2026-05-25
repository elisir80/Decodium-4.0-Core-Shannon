// CallsignHash28.h — 1.0.293 (fork iu8lmc), AP hashed-callsign cache Fase 0
//
// Hash28 CONDIVISO tra il bridge (seed della cache) e il decoder FT2 (lookup AP).
// CRITICO: bridge e Stage7 DEVONO usare questa identica funzione, altrimenti gli
// hash non coincidono mai e la cache è silenziosamente inerte.
//
// Contratto di normalizzazione (deve essere identico ai due lati):
//   - input: callsign in MAIUSCOLO e trimmato (la versione QString lo applica);
//   - campo fisso 13 char, pad con spazi, troncato a 13;
//   - nhash2 (jenkins) con seed 146, mascherato a 28 bit.
// Pattern nhash2 troncato verificato in Detector/FtxFt8Stage4.cpp:1540.
#pragma once

#include <cstdint>
#include <cstring>
#include <QByteArray>
#include <QString>

extern "C" uint32_t nhash2(void const* key, uint64_t length, uint32_t initval);

namespace decodium {

inline constexpr uint32_t kFt2HashSeed = 146u;
inline constexpr uint32_t kFt2Hash28Mask = 0x0FFFFFFFu;  // 28 bit
inline constexpr int      kFt2HashField = 13;

// Core: la call passata DEVE essere già MAIUSCOLA e trimmata.
inline quint32 ft2CallsignHash28Raw(const char* call, int len) noexcept
{
    char field[kFt2HashField];
    for (int i = 0; i < kFt2HashField; ++i)
        field[i] = (i < len) ? call[i] : ' ';
    return nhash2(field, static_cast<uint64_t>(kFt2HashField), kFt2HashSeed) & kFt2Hash28Mask;
}

// Wrapper QString: normalizza (upper + trim) prima di hashare.
inline quint32 ft2CallsignHash28(const QString& call)
{
    QByteArray const b = call.trimmed().toUpper().toLatin1();
    return ft2CallsignHash28Raw(b.constData(), b.size());
}

} // namespace decodium
