#include "flex/VitaPacket.h"

#include <QtEndian>

namespace decortty::flex {

namespace {

// Header word bit layout (VITA 49.2 §5.1.1), big-endian on the wire:
//   31..28 packet type      27 C (class ID present)   26 T (trailer present)
//   25..24 reserved         23..22 TSI                21..20 TSF
//   19..16 packet count     15..0  packet size in 32-bit words
constexpr uint32_t kClassIdPresentBit = 1u << 27;
constexpr uint32_t kTrailerPresentBit = 1u << 26;

uint32_t readU32(const uint8_t* p)
{
    return qFromBigEndian<quint32>(p);
}

} // namespace

bool parseVitaPacket(const uint8_t* data, int size, VitaPacket& out)
{
    if (!data || size < 4)
        return false;

    const uint32_t word0 = readU32(data);

    out = VitaPacket{};
    out.type        = static_cast<VitaPacketType>((word0 >> 28) & 0x0F);
    out.hasClassId  = (word0 & kClassIdPresentBit) != 0;
    out.hasTrailer  = (word0 & kTrailerPresentBit) != 0;
    out.tsi         = static_cast<Tsi>((word0 >> 22) & 0x03);
    out.tsf         = static_cast<Tsf>((word0 >> 20) & 0x03);
    out.packetCount = static_cast<uint8_t>((word0 >> 16) & 0x0F);
    out.sizeWords   = static_cast<uint16_t>(word0 & 0xFFFF);

    int offset = 4;

    if (out.hasStreamId()) {
        if (size < offset + 4)
            return false;
        out.streamId = readU32(data + offset);
        offset += 4;
    }

    if (out.hasClassId) {
        if (size < offset + 8)
            return false;
        // Word 2 holds the 24-bit OUI in its low bits; word 3 packs the
        // information class code above the packet class code.
        out.classId.oui              = readU32(data + offset) & 0x00FFFFFFu;
        const uint32_t classWord     = readU32(data + offset + 4);
        out.classId.informationClass = static_cast<uint16_t>(classWord >> 16);
        out.classId.packetClass      = static_cast<uint16_t>(classWord & 0xFFFFu);
        offset += 8;
    }

    if (out.tsi != Tsi::None) {
        if (size < offset + 4)
            return false;
        out.integerTimestamp = readU32(data + offset);
        offset += 4;
    }

    if (out.tsf != Tsf::None) {
        if (size < offset + 8)
            return false;
        out.fractionalTimestamp = (static_cast<uint64_t>(readU32(data + offset)) << 32)
                                | readU32(data + offset + 4);
        offset += 8;
    }

    int end = size;
    if (out.hasTrailer) {
        if (size < offset + 4)
            return false;
        end -= 4;
        out.trailer = readU32(data + end);
    }

    // The size field is authoritative when it fits inside what we actually
    // received; a datagram padded by the network stack would otherwise hand the
    // decoder a tail of zeros. A size field larger than the datagram means a
    // truncated packet, which we reject rather than read past.
    const int declared = static_cast<int>(out.sizeWords) * 4;
    if (declared > size)
        return false;
    if (declared >= offset && declared <= end)
        end = declared - (out.hasTrailer ? 4 : 0);

    if (end < offset)
        return false;

    out.payload      = data + offset;
    out.payloadBytes = end - offset;
    return true;
}

QByteArray buildFlexDataPacket(uint32_t   streamId,
                               FlexClass  packetClass,
                               const char* payload,
                               int         payloadBytes,
                               uint8_t     packetCount,
                               uint64_t    sampleCount)
{
    constexpr int kHeaderBytes = 28;   // header + stream ID + class ID + timestamps

    const int packetBytes = kHeaderBytes + payloadBytes;
    // The size field counts whole words even when the payload is not word
    // aligned — which is exactly what SmartSDR does for Opus.
    const uint16_t sizeWords = static_cast<uint16_t>((packetBytes + 3) / 4);

    QByteArray out(packetBytes, '\0');
    auto* w = reinterpret_cast<quint32*>(out.data());

    // Type 3 (ExtDataWithStream), class ID present, no trailer, TSI=Other,
    // TSF=SampleCount — the combination the radio's own clients send.
    const uint32_t word0 = (static_cast<uint32_t>(VitaPacketType::ExtDataWithStream) << 28)
                         | kClassIdPresentBit
                         | (static_cast<uint32_t>(Tsi::Other) << 22)
                         | (static_cast<uint32_t>(Tsf::SampleCount) << 20)
                         | (static_cast<uint32_t>(packetCount & 0x0F) << 16)
                         | sizeWords;

    w[0] = qToBigEndian<quint32>(word0);
    w[1] = qToBigEndian<quint32>(streamId);
    w[2] = qToBigEndian<quint32>(kFlexOui);
    w[3] = qToBigEndian<quint32>((static_cast<uint32_t>(kFlexInformationClass) << 16)
                                 | static_cast<uint32_t>(packetClass));
    // Timestamp intero: nessuno. TSI dice Other, cioe' "una scala che non e'
    // ne' UTC ne' GPS", e in un flusso audio fra due macchine sulla stessa rete
    // non c'e' niente di piu' onesto da metterci.
    w[4] = 0;
    // Timestamp frazionario: il conteggio dei campioni, su due parole.
    w[5] = qToBigEndian<quint32>(static_cast<quint32>(sampleCount >> 32));
    w[6] = qToBigEndian<quint32>(static_cast<quint32>(sampleCount & 0xFFFFFFFFull));

    if (payloadBytes > 0)
        memcpy(out.data() + kHeaderBytes, payload, static_cast<size_t>(payloadBytes));

    return out;
}

} // namespace decortty::flex
