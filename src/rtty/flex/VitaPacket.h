// DecoRTTY — VITA 49 (VRT) packet parsing and construction.
//
// The layout follows ANSI/VITA 49.2: a 32-bit header word, then the optional
// fields it advertises, in a fixed order — stream ID, class ID, integer
// timestamp, fractional timestamp — followed by the payload and an optional
// trailer word. Modelling the real standard rather than "28 bytes then data"
// costs nothing and means a packet that omits a field (or a future firmware
// that adds one) still parses correctly.
//
// FlexRadio uses one packet type for everything on the wire: ExtDataWithStream
// (type 3) with a class ID whose OUI is 0x001C2D and whose information class is
// 0x534C ("SL", for SmartLink). The packet class code inside that class ID is
// what actually distinguishes audio from FFT bins from meter readings.
#pragma once

#include <QByteArray>

#include <cstdint>

namespace decortty::flex {

enum class VitaPacketType : uint8_t {
    IfData             = 0,
    IfDataWithStream   = 1,
    ExtData            = 2,
    ExtDataWithStream  = 3,
    IfContext          = 4,
    ExtContext         = 5,
    Command            = 6,
    ExtCommand         = 7,
};

// Timestamp modes from the header word.
enum class Tsi : uint8_t { None = 0, Utc = 1, Gps = 2, Other = 3 };
enum class Tsf : uint8_t { None = 0, SampleCount = 1, RealTime = 2, FreeRunning = 3 };

// FlexRadio class-ID constants.
inline constexpr uint32_t kFlexOui = 0x00001C2D;
inline constexpr uint16_t kFlexInformationClass = 0x534C;

// Packet class codes, the field that says what a payload actually is.
enum class FlexClass : uint16_t {
    Unknown            = 0x0000,
    DaxAudioReducedBw  = 0x0123,   // int16 mono, big-endian — DAX a banda ridotta
    DaxAudioReducedTx  = 0x0124,   // la stessa cosa, verso la radio
    // float32 stereo big-endian a 24 kHz. E' la classe di ogni audio non
    // compresso che passa fra client e radio: il flusso DAX di una slice e
    // l'audio "cuffie" del client remoto hanno lo stesso involucro, e cambia
    // solo il comando che li ha creati.
    DaxAudio           = 0x03E3,
    Meter              = 0x8002,   // (uint16 id, int16 value) pairs
    Fft                = 0x8003,   // panadapter bins
    Waterfall          = 0x8004,   // waterfall tiles
    Opus               = 0x8005,   // Opus-compressed audio
    Discovery          = 0xFFFF,   // discovery broadcast, ASCII key=value payload
};

struct VitaClassId {
    uint32_t oui{0};
    uint16_t informationClass{0};
    uint16_t packetClass{0};

    bool isFlex() const { return oui == kFlexOui; }
    FlexClass flexClass() const { return static_cast<FlexClass>(packetClass); }
};

// A parsed packet. `payload` points into the caller's buffer — it is a view,
// not a copy, so it stays valid only as long as the datagram does.
struct VitaPacket {
    VitaPacketType type{VitaPacketType::ExtDataWithStream};
    bool           hasClassId{false};
    bool           hasTrailer{false};
    Tsi            tsi{Tsi::None};
    Tsf            tsf{Tsf::None};
    uint8_t        packetCount{0};    // 4-bit modulo-16 sequence counter
    uint16_t       sizeWords{0};      // whole packet, in 32-bit words

    uint32_t       streamId{0};
    VitaClassId    classId;
    uint32_t       integerTimestamp{0};
    uint64_t       fractionalTimestamp{0};

    const uint8_t* payload{nullptr};
    int            payloadBytes{0};
    uint32_t       trailer{0};

    bool hasStreamId() const
    {
        return type == VitaPacketType::IfDataWithStream
            || type == VitaPacketType::ExtDataWithStream
            || type >= VitaPacketType::IfContext;
    }
};

// Parse a datagram. Returns false when the buffer is too short or the header
// advertises more than it contains.
bool parseVitaPacket(const uint8_t* data, int size, VitaPacket& out);
inline bool parseVitaPacket(const QByteArray& data, VitaPacket& out)
{
    return parseVitaPacket(reinterpret_cast<const uint8_t*>(data.constData()), data.size(), out);
}

// Build an ExtDataWithStream packet carrying `payload` verbatim — the shape the
// radio expects for client audio. The payload is not padded: FlexRadio's own
// Opus packets are byte-length, while the header's size field stays in whole
// 32-bit words as the standard requires.
//
// `sampleCount` e' il timestamp frazionario: il numero d'ordine del primo
// campione del pacchetto dentro il flusso, che avanza e non torna indietro.
// L'intestazione dichiara TSF=SampleCount, e dichiararlo mandando zeri sarebbe
// dire una cosa e farne un'altra — chi riceve non ha modo di accorgersi che il
// campo e' finto, e chi lo usa per rimettere in fila i pacchetti sbaglia.
QByteArray buildFlexDataPacket(uint32_t   streamId,
                               FlexClass  packetClass,
                               const char* payload,
                               int         payloadBytes,
                               uint8_t     packetCount,
                               uint64_t    sampleCount = 0);

// Convenience for the audio path.
inline QByteArray buildFlexDataPacket(uint32_t streamId,
                                      FlexClass packetClass,
                                      const QByteArray& payload,
                                      uint8_t packetCount,
                                      uint64_t sampleCount = 0)
{
    return buildFlexDataPacket(streamId, packetClass, payload.constData(), payload.size(),
                               packetCount, sampleCount);
}

} // namespace decortty::flex
