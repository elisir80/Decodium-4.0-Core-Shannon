#include "flex/OpusCodec.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

#ifdef DECORTTY_HAVE_OPUS
#include <opus/opus.h>
#endif

namespace decortty::flex {

namespace {
constexpr int kMaxFrameBytes = 4000;
// SmartSDR runs its audio streams at about 70 kbit/s. Matching it keeps the
// radio's jitter buffer behaving the way it does for the vendor's own client.
constexpr int kBitrate = 70000;

// Serve solo quando Opus c'e': senza, resta orfana e il compilatore lo
// segnala. Marcata invece che spostata nell'#ifdef, per tenerla leggibile
// accanto alle costanti del codec.
[[maybe_unused]] qint16 toPcm(float v)
{
    const float clamped = std::clamp(v, -1.0f, 1.0f);
    return static_cast<qint16>(std::lround(clamped * 32767.0f));
}
} // namespace

OpusCodec::OpusCodec()
{
#ifdef DECORTTY_HAVE_OPUS
    int err = 0;
    m_encoder = opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK) {
        m_encoder = nullptr;
    } else {
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(kBitrate));
        opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(10));
        // RTTY is a pair of steady tones, not speech. Telling Opus so stops the
        // speech model from smearing the shift, which would cost real decode
        // margin at the far end.
        opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    }

    m_decoder = opus_decoder_create(kSampleRate, kChannels, &err);
    if (err != OPUS_OK)
        m_decoder = nullptr;
#endif
}

OpusCodec::~OpusCodec()
{
#ifdef DECORTTY_HAVE_OPUS
    if (m_encoder)
        opus_encoder_destroy(m_encoder);
    if (m_decoder)
        opus_decoder_destroy(m_decoder);
#endif
}

bool OpusCodec::isAvailable() const
{
#ifdef DECORTTY_HAVE_OPUS
    return m_encoder != nullptr && m_decoder != nullptr;
#else
    return false;
#endif
}

void OpusCodec::reset()
{
    m_accumulator.clear();
}

std::vector<QByteArray> OpusCodec::encodeMono(const float* samples, int count)
{
    std::vector<QByteArray> frames;
#ifdef DECORTTY_HAVE_OPUS
    if (!m_encoder || count <= 0)
        return frames;

    m_accumulator.reserve(m_accumulator.size() + static_cast<size_t>(count) * 2);
    for (int i = 0; i < count; ++i) {
        const qint16 pcm = toPcm(samples[i]);
        m_accumulator.push_back(pcm);   // left
        m_accumulator.push_back(pcm);   // right
    }

    constexpr size_t frameValues = static_cast<size_t>(kFrameSize) * kChannels;
    QByteArray encoded(kMaxFrameBytes, Qt::Uninitialized);

    while (m_accumulator.size() >= frameValues) {
        const int bytes = opus_encode(m_encoder,
                                      m_accumulator.data(),
                                      kFrameSize,
                                      reinterpret_cast<unsigned char*>(encoded.data()),
                                      kMaxFrameBytes);
        m_accumulator.erase(m_accumulator.begin(),
                            m_accumulator.begin() + static_cast<long>(frameValues));
        if (bytes > 0)
            frames.push_back(encoded.left(bytes));
    }
#else
    Q_UNUSED(samples);
    Q_UNUSED(count);
#endif
    return frames;
}

std::vector<float> OpusCodec::decode(const QByteArray& frame)
{
    std::vector<float> out;
#ifdef DECORTTY_HAVE_OPUS
    if (!m_decoder)
        return out;

    out.resize(static_cast<size_t>(kFrameSize) * kChannels);
    const int frames = opus_decode_float(m_decoder,
                                         reinterpret_cast<const unsigned char*>(frame.constData()),
                                         static_cast<opus_int32>(frame.size()),
                                         out.data(),
                                         kFrameSize,
                                         0);
    if (frames <= 0) {
        out.clear();
        return out;
    }
    out.resize(static_cast<size_t>(frames) * kChannels);
#else
    Q_UNUSED(frame);
#endif
    return out;
}

} // namespace decortty::flex
