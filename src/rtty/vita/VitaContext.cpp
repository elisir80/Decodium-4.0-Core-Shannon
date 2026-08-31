#include "vita/VitaContext.h"

#include <QtEndian>

#include <cmath>
#include <cstring>

namespace decortty::vita {

using flex::VitaPacket;
using flex::VitaPacketType;

namespace {

constexpr int kHeaderBytes = 28;   // header + stream ID + class ID + timestamps

// Vendor extension bitmap, carried after the standard CIF0 fields in a DecoRTTY
// command packet. Kept in its own word so the standard part of the payload stays
// exactly what a conforming parser expects.
constexpr uint32_t kVendorMode     = 1u << 31;
constexpr uint32_t kVendorTransmit = 1u << 30;
constexpr int      kModeFieldBytes = 8;   // fixed-width ASCII, NUL padded

void appendU32(QByteArray& out, uint32_t value)
{
    const quint32 be = qToBigEndian<quint32>(value);
    out.append(reinterpret_cast<const char*>(&be), 4);
}

void appendI64(QByteArray& out, qint64 value)
{
    const qint64 be = qToBigEndian<qint64>(value);
    out.append(reinterpret_cast<const char*>(&be), 8);
}

// Build the 28-byte prefix shared by every packet DecoRTTY emits.
QByteArray buildHeader(VitaPacketType type, uint32_t streamId, DecoClass packetClass,
                       uint8_t packetCount, int totalBytes)
{
    QByteArray header(kHeaderBytes, '\0');
    auto* w = reinterpret_cast<quint32*>(header.data());

    const uint16_t sizeWords = static_cast<uint16_t>((totalBytes + 3) / 4);
    const uint32_t word0 = (static_cast<uint32_t>(type) << 28)
                         | (1u << 27)                                     // class ID present
                         | (static_cast<uint32_t>(flex::Tsi::Other) << 22)
                         | (static_cast<uint32_t>(flex::Tsf::SampleCount) << 20)
                         | (static_cast<uint32_t>(packetCount & 0x0F) << 16)
                         | sizeWords;

    w[0] = qToBigEndian<quint32>(word0);
    w[1] = qToBigEndian<quint32>(streamId);
    w[2] = qToBigEndian<quint32>(kDecoRttyOui);
    w[3] = qToBigEndian<quint32>((static_cast<uint32_t>(kDecoRttyInformationClass) << 16)
                                 | static_cast<uint32_t>(packetClass));
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
    return header;
}

bool isDecoRtty(const VitaPacket& packet, DecoClass expected)
{
    return packet.hasClassId
        && packet.classId.oui == kDecoRttyOui
        && packet.classId.packetClass == static_cast<uint16_t>(expected);
}

} // namespace

qint64 toFixed64Radix20(double value)
{
    return static_cast<qint64>(std::llround(value * 1048576.0));   // 2^20
}

double fromFixed64Radix20(qint64 raw)
{
    return static_cast<double>(raw) / 1048576.0;
}

qint16 toFixed16Radix7(double value)
{
    return static_cast<qint16>(std::lround(value * 128.0));        // 2^7
}

double fromFixed16Radix7(qint16 raw)
{
    return static_cast<double>(raw) / 128.0;
}

// ── context ─────────────────────────────────────────────────────────────────

QByteArray buildContextPacket(uint32_t streamId, const RadioContext& context,
                              uint8_t packetCount)
{
    uint32_t cif0 = Cif0ContextFieldChanged;
    QByteArray fields;

    // Fields must appear in CIF0 bit order, most significant first — the
    // receiver walks the bitmap and consumes them in exactly that sequence.
    if (context.bandwidthHz) {
        cif0 |= Cif0Bandwidth;
        appendI64(fields, toFixed64Radix20(*context.bandwidthHz));
    }
    if (context.rfFrequencyHz) {
        cif0 |= Cif0RfRefFrequency;
        appendI64(fields, toFixed64Radix20(*context.rfFrequencyHz));
    }
    if (context.referenceLevelDbm) {
        cif0 |= Cif0ReferenceLevel;
        // 32-bit field: upper half reserved, lower half the radix-7 value.
        appendU32(fields, static_cast<uint32_t>(
                              static_cast<uint16_t>(toFixed16Radix7(*context.referenceLevelDbm))));
    }
    if (context.gainDb) {
        cif0 |= Cif0Gain;
        // Stage 2 gain in the upper half, stage 1 in the lower. We report a
        // single figure, so it goes in stage 1 and stage 2 stays zero.
        appendU32(fields, static_cast<uint32_t>(
                              static_cast<uint16_t>(toFixed16Radix7(*context.gainDb))));
    }
    if (context.sampleRateHz) {
        cif0 |= Cif0SampleRate;
        appendI64(fields, toFixed64Radix20(*context.sampleRateHz));
    }
    if (context.stateAndEvent) {
        cif0 |= Cif0StateAndEvent;
        appendU32(fields, *context.stateAndEvent);
    }

    const int totalBytes = kHeaderBytes + 4 + fields.size();
    QByteArray out = buildHeader(VitaPacketType::IfContext, streamId,
                                 DecoClass::RadioContext, packetCount, totalBytes);
    appendU32(out, cif0);
    out.append(fields);
    return out;
}

bool parseContextPacket(const VitaPacket& packet, RadioContext& out)
{
    if (packet.type != VitaPacketType::IfContext)
        return false;
    if (!isDecoRtty(packet, DecoClass::RadioContext))
        return false;
    if (packet.payloadBytes < 4)
        return false;

    const uint8_t* p   = packet.payload;
    int            left = packet.payloadBytes;

    const uint32_t cif0 = qFromBigEndian<quint32>(p);
    p    += 4;
    left -= 4;

    const auto takeI64 = [&](double& target) {
        if (left < 8)
            return false;
        target = fromFixed64Radix20(static_cast<qint64>(qFromBigEndian<quint64>(p)));
        p    += 8;
        left -= 8;
        return true;
    };
    const auto takeU32 = [&](uint32_t& target) {
        if (left < 4)
            return false;
        target = qFromBigEndian<quint32>(p);
        p    += 4;
        left -= 4;
        return true;
    };

    out = RadioContext{};

    if (cif0 & Cif0Bandwidth) {
        double v = 0.0;
        if (!takeI64(v)) return false;
        out.bandwidthHz = v;
    }
    if (cif0 & Cif0RfRefFrequency) {
        double v = 0.0;
        if (!takeI64(v)) return false;
        out.rfFrequencyHz = v;
    }
    if (cif0 & Cif0ReferenceLevel) {
        uint32_t v = 0;
        if (!takeU32(v)) return false;
        out.referenceLevelDbm = fromFixed16Radix7(static_cast<qint16>(v & 0xFFFFu));
    }
    if (cif0 & Cif0Gain) {
        uint32_t v = 0;
        if (!takeU32(v)) return false;
        out.gainDb = fromFixed16Radix7(static_cast<qint16>(v & 0xFFFFu));
    }
    if (cif0 & Cif0SampleRate) {
        double v = 0.0;
        if (!takeI64(v)) return false;
        out.sampleRateHz = v;
    }
    if (cif0 & Cif0StateAndEvent) {
        uint32_t v = 0;
        if (!takeU32(v)) return false;
        out.stateAndEvent = v;
    }
    return true;
}

// ── commands ────────────────────────────────────────────────────────────────

QByteArray buildCommandPacket(uint32_t streamId, const RadioCommand& command,
                              uint8_t packetCount)
{
    uint32_t cif0   = 0;
    uint32_t vendor = 0;
    QByteArray standardFields;
    QByteArray vendorFields;

    if (command.bandwidthHz) {
        cif0 |= Cif0Bandwidth;
        appendI64(standardFields, toFixed64Radix20(*command.bandwidthHz));
    }
    if (command.tuneFrequencyHz) {
        cif0 |= Cif0RfRefFrequency;
        appendI64(standardFields, toFixed64Radix20(*command.tuneFrequencyHz));
    }
    if (command.mode) {
        vendor |= kVendorMode;
        QByteArray token = command.mode->toLatin1().left(kModeFieldBytes);
        token.append(kModeFieldBytes - token.size(), '\0');
        vendorFields.append(token);
    }
    if (command.transmit) {
        vendor |= kVendorTransmit;
        appendU32(vendorFields, *command.transmit ? 1u : 0u);
    }

    const int totalBytes = kHeaderBytes + 8 + standardFields.size() + vendorFields.size();
    QByteArray out = buildHeader(VitaPacketType::Command, streamId,
                                 DecoClass::Command, packetCount, totalBytes);
    appendU32(out, cif0);
    appendU32(out, vendor);
    out.append(standardFields);
    out.append(vendorFields);
    return out;
}

bool parseCommandPacket(const VitaPacket& packet, RadioCommand& out)
{
    if (packet.type != VitaPacketType::Command)
        return false;
    if (!isDecoRtty(packet, DecoClass::Command))
        return false;
    if (packet.payloadBytes < 8)
        return false;

    const uint8_t* p    = packet.payload;
    int            left = packet.payloadBytes;

    const uint32_t cif0   = qFromBigEndian<quint32>(p);
    const uint32_t vendor = qFromBigEndian<quint32>(p + 4);
    p    += 8;
    left -= 8;

    out = RadioCommand{};

    if (cif0 & Cif0Bandwidth) {
        if (left < 8) return false;
        out.bandwidthHz = fromFixed64Radix20(static_cast<qint64>(qFromBigEndian<quint64>(p)));
        p += 8; left -= 8;
    }
    if (cif0 & Cif0RfRefFrequency) {
        if (left < 8) return false;
        out.tuneFrequencyHz = fromFixed64Radix20(static_cast<qint64>(qFromBigEndian<quint64>(p)));
        p += 8; left -= 8;
    }
    if (vendor & kVendorMode) {
        if (left < kModeFieldBytes) return false;
        // The token is NUL padded; stop at the first NUL rather than carrying
        // the padding into the mode string.
        const char* text = reinterpret_cast<const char*>(p);
        int len = 0;
        while (len < kModeFieldBytes && text[len] != '\0')
            ++len;
        out.mode = QString::fromLatin1(text, len);
        p += kModeFieldBytes; left -= kModeFieldBytes;
    }
    if (vendor & kVendorTransmit) {
        if (left < 4) return false;
        out.transmit = qFromBigEndian<quint32>(p) != 0;
        p += 4; left -= 4;
    }
    return true;
}

} // namespace decortty::vita
