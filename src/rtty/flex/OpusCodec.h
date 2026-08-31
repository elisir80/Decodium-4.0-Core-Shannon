// DecoRTTY — Opus wrapper matching SmartSDR's audio configuration.
//
// The radio accepts client transmit audio only as Opus (stereo, 24 kHz, 10 ms
// frames), so this is a hard requirement of the transmit path rather than a
// bandwidth optimisation. Receive audio is normally uncompressed float32, but
// the radio switches to Opus on a routed or SmartLink connection, so the decode
// direction is implemented too.
#pragma once

#include <QByteArray>

#include <vector>

struct OpusEncoder;
struct OpusDecoder;

namespace decortty::flex {

class OpusCodec {
public:
    static constexpr int kSampleRate = 24000;
    static constexpr int kChannels   = 2;
    static constexpr int kFrameMs    = 10;
    static constexpr int kFrameSize  = kSampleRate * kFrameMs / 1000;   // 240

    OpusCodec();
    ~OpusCodec();

    OpusCodec(const OpusCodec&)            = delete;
    OpusCodec& operator=(const OpusCodec&) = delete;

    bool isAvailable() const;

    // Mono float from the modulator → one Opus frame. Feeds an internal
    // accumulator, so callers may hand over any block size; returns as many
    // whole frames as the input allowed.
    std::vector<QByteArray> encodeMono(const float* samples, int count);

    // Opus frame → interleaved stereo float at 24 kHz.
    std::vector<float> decode(const QByteArray& frame);

    void reset();

private:
    OpusEncoder*       m_encoder{nullptr};
    OpusDecoder*       m_decoder{nullptr};
    std::vector<qint16> m_accumulator;   // interleaved stereo, awaiting a frame
};

} // namespace decortty::flex
