// DecoRTTY — VITA 49.2 context and command packets.
//
// A signal-data packet says what the samples are; a *context* packet says what
// they mean — the tuned frequency, the bandwidth, the sample rate. VITA 49.2
// models exactly that with the Context Indicator Field (CIF0), a bitmap where
// each set bit adds one fixed-size field to the payload, in bit order from the
// most significant down.
//
// This is what lets a plain HF transceiver be presented as a network SDR: the
// gateway sends signal-data packets for the audio and context packets for the
// dial frequency and mode, and the client never learns there is a serial cable
// at the other end.
//
// The CIF0 fields used here are the standard ones. Everything the standard does
// not model — an amateur transceiver's operating mode, its PTT line — travels in
// a separate vendor context packet under DecoRTTY's own class ID, rather than
// being smuggled into a standard field with a private meaning.
#pragma once

#include "flex/VitaPacket.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace decortty::vita {

// Locally administered organisation identifier for DecoRTTY's own packet
// classes. Not an IEEE-registered OUI: it exists to keep our packets
// distinguishable from a real radio's on the same wire, and is documented as
// local so nobody mistakes it for an assignment.
inline constexpr uint32_t kDecoRttyOui = 0x00DEC0;
inline constexpr uint16_t kDecoRttyInformationClass = 0x4454;   // "DT"

enum class DecoClass : uint16_t {
    Discovery    = 0x0001,   // beacon, ASCII key=value payload
    RadioContext = 0x0002,   // vendor state: mode, PTT, S-meter
    Command      = 0x0003,   // client → gateway control
    Audio        = 0x03E3,   // float32 stereo big-endian, same layout as the
                             // uncompressed remote-audio format, so one decoder
                             // path serves both transports
};

// ── CIF0 ────────────────────────────────────────────────────────────────────
// Bit positions, counted from the most significant bit of the CIF0 word.
enum Cif0Bit : uint32_t {
    Cif0ContextFieldChanged = 1u << 31,
    Cif0ReferencePointId    = 1u << 30,
    Cif0Bandwidth           = 1u << 29,
    Cif0IfRefFrequency      = 1u << 28,
    Cif0RfRefFrequency      = 1u << 27,
    Cif0RfRefFreqOffset     = 1u << 26,
    Cif0IfBandOffset        = 1u << 25,
    Cif0ReferenceLevel      = 1u << 24,
    Cif0Gain                = 1u << 23,
    Cif0OverRangeCount      = 1u << 22,
    Cif0SampleRate          = 1u << 21,
    Cif0TimestampAdjustment = 1u << 20,
    Cif0TimestampCalTime    = 1u << 19,
    Cif0Temperature         = 1u << 18,
    Cif0DeviceIdentifier    = 1u << 17,
    Cif0StateAndEvent       = 1u << 16,
    Cif0PayloadFormat       = 1u << 15,
};

// State and Event Indicator bits. The standard defines the upper enables and
// indicators; bits 19..0 are explicitly user-defined, which is where the
// transmit state belongs — it is a real state of the device, not an invented
// field.
inline constexpr uint32_t kStateEnableCalibratedTime = 1u << 31;
inline constexpr uint32_t kStateEnableValidData      = 1u << 30;
inline constexpr uint32_t kStateEnableReferenceLock  = 1u << 29;
inline constexpr uint32_t kStateValidData            = 1u << 18;
inline constexpr uint32_t kStateReferenceLock        = 1u << 17;
// User-defined region.
inline constexpr uint32_t kStateUserTransmitting     = 1u << 0;
inline constexpr uint32_t kStateUserCatOnline        = 1u << 1;

// What a context packet can carry. Every field is optional: a context packet
// reports what changed, and an absent field means "unchanged", never "zero".
struct RadioContext {
    std::optional<double>   rfFrequencyHz;
    std::optional<double>   bandwidthHz;
    std::optional<double>   sampleRateHz;
    std::optional<double>   referenceLevelDbm;
    std::optional<double>   gainDb;
    std::optional<uint32_t> stateAndEvent;

    bool empty() const
    {
        return !rfFrequencyHz && !bandwidthHz && !sampleRateHz
            && !referenceLevelDbm && !gainDb && !stateAndEvent;
    }
};

// Build an IF Context packet (VITA 49.2 type 4) carrying `context`.
QByteArray buildContextPacket(uint32_t streamId, const RadioContext& context,
                              uint8_t packetCount);

// Decode an IF Context packet's payload. Returns false when the packet is not a
// context packet or its CIF0 promises more than the payload holds.
bool parseContextPacket(const flex::VitaPacket& packet, RadioContext& out);

// ── commands ────────────────────────────────────────────────────────────────
// Control travelling the other way. VITA 49.2's full command format carries a
// controllee identifier, an acknowledgement mode and its own CIF set; DecoRTTY
// uses a deliberately small subset — a command packet whose payload is a CIF0
// bitmap plus, where the standard has no field for what an HF transceiver needs,
// a vendor extension word. The subset is documented rather than dressed up as
// full compliance.
struct RadioCommand {
    std::optional<double>  tuneFrequencyHz;
    std::optional<double>  bandwidthHz;
    // Vendor extension: an amateur transceiver's operating mode, as a short
    // ASCII token ("USB", "DIGU", "RTTY-U"), and the transmit request.
    std::optional<QString> mode;
    std::optional<bool>    transmit;

    bool empty() const
    {
        return !tuneFrequencyHz && !bandwidthHz && !mode && !transmit;
    }
};

QByteArray buildCommandPacket(uint32_t streamId, const RadioCommand& command,
                              uint8_t packetCount);
bool parseCommandPacket(const flex::VitaPacket& packet, RadioCommand& out);

// ── fixed-point helpers ─────────────────────────────────────────────────────
// VITA 49 carries frequencies, bandwidths and sample rates as 64-bit
// two's-complement fixed point with the radix point after bit 20, giving just
// under a microhertz of resolution over the full range. Reference level and gain
// are 16-bit fields with the radix after bit 7.
qint64 toFixed64Radix20(double value);
double fromFixed64Radix20(qint64 raw);
qint16 toFixed16Radix7(double value);
double fromFixed16Radix7(qint16 raw);

} // namespace decortty::vita
