// DecoPort v1 - codifica e decodifica dei pacchetti.
//
// Il protocollo e' descritto in doc/DECOPORT_PROTOCOL.md. Qui c'e' solo la
// serializzazione: nessuna conoscenza della radio, nessuna I/O. E' voluto —
// questo file deve restare provabile da solo.
//
// Tutto l'header e i campi di contesto sono big-endian; l'audio PCM e'
// little-endian, che e' come lo trattano le schede audio e come lo vuole il
// resto di Decodium.
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace decoport {

inline constexpr quint32 kMagic = 0x44505254u;   // 'DPRT'
inline constexpr quint8  kVersion = 1;
inline constexpr int     kHeaderBytes = 28;
inline constexpr quint16 kSessionPort = 5559;
inline constexpr quint16 kAnnouncePort = 5560;
inline constexpr int     kAudioFrameMs = 10;
inline constexpr int     kDefaultSampleRate = 48000;
inline constexpr int     kClientTimeoutMs = 12000;
inline constexpr int     kAnnounceIntervalMs = 2000;
// Firma troncata a 16 byte: su un pacchetto audio da 10 ms sono 16 byte ogni
// 240, e per un falsario 128 bit restano fuori portata.
inline constexpr int     kAuthTagBytes = 16;
// Fuori da questa finestra un pacchetto e' vecchio e viene buttato: e' cio' che
// impedisce di registrare un comando e rigiocarlo piu' tardi.
inline constexpr int     kReplayWindowSeconds = 30;

enum class Type : quint8 {
    Announce  = 1,
    Hello     = 2,
    Bye       = 3,
    KeepAlive = 4,
    Context   = 5,
    Command   = 6,
    AudioRx   = 7,
    AudioTx   = 8,
    Status    = 9,
};

enum Flags : quint16 {
    FlagNone          = 0,
    FlagHasTimestamp  = 1u << 0,
    // Il pacchetto porta in coda la firma HMAC-SHA256 troncata.
    FlagAuthenticated = 1u << 1,
};

// Modi neutri. DIGU/DIGL non sono nomi di pannello: significano "il modo in cui
// il codec USB entra nel modulatore", e come si chiami sulla radio trovata e'
// un problema del gateway.
enum class Mode : quint8 {
    Unknown = 0,
    Usb     = 1,
    Lsb     = 2,
    Cw      = 3,
    Cwr     = 4,
    Am      = 5,
    Fm      = 6,
    Digu    = 7,
    Digl    = 8,
    Rtty    = 9,
    Rttyr   = 10,
    PktFm   = 11,
};

QString  modeToString(Mode m);
Mode     modeFromString(const QString& s);

// Bit della maschera dei campi di contesto.
enum ContextField : quint32 {
    FieldFrequency      = 1u << 0,
    FieldMode           = 1u << 1,
    FieldPtt            = 1u << 2,
    FieldSMeter         = 1u << 3,
    FieldSampleRate     = 1u << 4,
    FieldChannels       = 1u << 5,
    FieldBandwidth      = 1u << 6,
    FieldRigLabel       = 1u << 7,
    FieldStateFlags     = 1u << 8,
    FieldTxAudioLeadMs  = 1u << 9,
    FieldSessionPort    = 1u << 10,
};

enum StateFlag : quint32 {
    StateCatOnline    = 1u << 0,
    StateAudioIn      = 1u << 1,
    StateAudioOut     = 1u << 2,
    StateCanTransmit  = 1u << 3,
    StateTxHeld       = 1u << 4,
};

struct Header {
    quint8  version {kVersion};
    Type    type {Type::Status};
    quint16 flags {FlagNone};
    quint32 streamId {0};
    quint32 sequence {0};
    quint32 tsSeconds {0};
    quint32 tsNanos {0};
    quint16 payloadLength {0};
};

// Un contesto: la maschera dice quali campi valgono. Un CONTEXT li nomina tutti,
// un COMMAND solo quelli che vuole cambiare — stessa struttura per entrambi, che
// e' proprio il punto.
struct Context {
    quint32 mask {0};
    qint64  frequencyHz {0};
    Mode    mode {Mode::Unknown};
    bool    ptt {false};
    qint16  sMeterDbmTenths {0};
    quint32 sampleRate {kDefaultSampleRate};
    quint8  channels {1};
    quint32 bandwidthHz {0};
    QString rigLabel;
    quint32 stateFlags {0};
    quint16 txAudioLeadMs {0};
    quint16 sessionPort {kSessionPort};

    bool has(ContextField f) const { return (mask & static_cast<quint32>(f)) != 0; }

    void setFrequency(qint64 hz)      { frequencyHz = hz;  mask |= FieldFrequency; }
    void setMode(Mode m)              { mode = m;          mask |= FieldMode; }
    void setPtt(bool on)              { ptt = on;          mask |= FieldPtt; }
    void setSMeterDbm(double dbm);
    void setSampleRate(quint32 r)     { sampleRate = r;    mask |= FieldSampleRate; }
    void setChannels(quint8 c)        { channels = c;      mask |= FieldChannels; }
    void setBandwidth(quint32 hz)     { bandwidthHz = hz;  mask |= FieldBandwidth; }
    void setRigLabel(const QString& s){ rigLabel = s;      mask |= FieldRigLabel; }
    void setStateFlags(quint32 f)     { stateFlags = f;    mask |= FieldStateFlags; }
    void setTxAudioLeadMs(quint16 ms) { txAudioLeadMs = ms;mask |= FieldTxAudioLeadMs; }
    void setSessionPort(quint16 p)    { sessionPort = p;   mask |= FieldSessionPort; }

    double sMeterDbm() const { return static_cast<double>(sMeterDbmTenths) / 10.0; }
};

// ── serializzazione ──────────────────────────────────────────────────────────

QByteArray encodeContextPayload(const Context& ctx);
bool       decodeContextPayload(const QByteArray& payload, Context* out);

// Costruisce un pacchetto completo. `tsNs` e' il tempo Unix in nanosecondi: per
// AUDIO_TX e per un COMMAND che aziona il PTT e' l'istante in cui la cosa deve
// ACCADERE, non quando e' stata spedita.
// Con authKey non vuota il pacchetto viene firmato e la firma appesa in coda;
// payloadLength continua a contare il solo payload.
QByteArray buildPacket(Type type, quint32 streamId, quint32 sequence,
                       quint64 tsNs, const QByteArray& payload,
                       const QByteArray& authKey = QByteArray());

// Legge l'header e restituisce il payload. Rifiuta magic, versione e lunghezze
// che non tornano: da un socket UDP arriva di tutto.
// `authenticated` dice se la firma c'era ED e' giusta. Chi riceve decide cosa
// farne: qui non si scarta nulla di nascosto.
bool parsePacket(const QByteArray& datagram, Header* header, QByteArray* payload,
                 const QByteArray& authKey = QByteArray(),
                 bool* authenticated = nullptr);

// Chiave dalla password. DEVE essere deterministica: due macchine devono
// ricavare la stessa chiave dalla stessa parola, quindi il sale e' fisso e non
// casuale. In cambio si alza il numero di iterazioni, cosi' provare le password
// a tentativi costa comunque caro.
QByteArray deriveKeyFromPassword(const QString& password);

// Vero se il timestamp del pacchetto sta dentro la finestra anti-replay.
bool timestampAcceptable(const Header& h, qint64 windowSeconds = kReplayWindowSeconds);

quint64 nowUnixNs();

} // namespace decoport
