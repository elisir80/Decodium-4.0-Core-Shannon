#include "lib/ft2link/FT2LinkWaveform.hpp"

#include "lib/ft2link/FT2LinkPhysical.hpp"

#include <algorithm>
#include <cmath>

namespace decodium
{
namespace ft2link
{
namespace
{
constexpr double kPi {3.141592653589793238462643383279502884};
constexpr std::size_t kLengthSymbols {8};
constexpr std::size_t kW2300LengthSymbols {2};
constexpr std::size_t kW2300ModeSymbols {3};
constexpr std::size_t kW2300Subcarriers {4};
constexpr std::uint8_t kW2300FastModeSymbol {0x46u};
constexpr std::uint8_t kW2300RobustModeSymbol {0x52u};
constexpr std::uint8_t kNarrowPacketMagic0 {'N'};
constexpr std::uint8_t kNarrowPacketMagic1 {'2'};
constexpr std::uint8_t kNarrowPacketVersion {1};

struct ToneBasis
{
  std::vector<std::vector<double> > i;
  std::vector<std::vector<double> > q;
};

struct SubcarrierBasis
{
  std::vector<std::vector<double> > i;
  std::vector<std::vector<double> > q;
};

struct SymbolDecision
{
  std::uint8_t symbol {0};
  double bestPower {0.0};
  double secondPower {0.0};
};

struct DecodeCandidate
{
  bool ok {false};
  std::vector<std::uint8_t> packet;
  W500DecodeMetrics metrics;
};

struct W2300SymbolState
{
  double i[kW2300Subcarriers] {0.0, 0.0, 0.0, 0.0};
  double q[kW2300Subcarriers] {0.0, 0.0, 0.0, 0.0};
};

struct W2300SymbolDecision
{
  std::uint8_t symbol {0};
  double quality {0.0};
};

struct W2300DecodeCandidate
{
  bool ok {false};
  std::vector<std::uint8_t> packet;
  W2300DecodeMetrics metrics;
};

struct NarrowToneBasis
{
  std::vector<std::vector<double> > i;
  std::vector<std::vector<double> > q;
};

struct NarrowSymbolDecision
{
  std::uint8_t bit {0};
  double bestPower {0.0};
  double secondPower {0.0};
};

struct NarrowDecodeCandidate
{
  bool ok {false};
  Frame frame;
  NarrowDecodeMetrics metrics;
};

struct W2300BurstInfo
{
  W2300RateMode mode {W2300RateMode::Fast};
  int repetitionFactor {1};
  std::uint16_t packetLength {0};
  std::size_t payloadOffset {0};
  std::size_t requiredSymbols {0};
};

std::vector<std::uint8_t> const& preambleSymbols ()
{
  static std::vector<std::uint8_t> const symbols {
    0, 3, 0, 3, 0, 3, 0, 3,
    3, 0, 3, 0, 3, 0, 3, 0
  };
  return symbols;
}

std::vector<std::uint8_t> const& syncSymbols ()
{
  static std::vector<std::uint8_t> const symbols {0, 1, 3, 2, 3, 1, 0, 2};
  return symbols;
}

std::vector<std::uint8_t> const& w2300PreambleSymbols ()
{
  static std::vector<std::uint8_t> const symbols {
    0x00u, 0xffu, 0x00u, 0xffu,
    0x33u, 0xccu, 0x33u, 0xccu,
    0x55u, 0xaau, 0x55u, 0xaau,
    0x0fu, 0xf0u, 0x0fu, 0xf0u
  };
  return symbols;
}

std::vector<std::uint8_t> const& w2300SyncSymbols ()
{
  static std::vector<std::uint8_t> const symbols {
    0xd2u, 0xb4u, 0xe1u, 0x78u,
    0x4bu, 0x96u, 0x2du, 0xc3u
  };
  return symbols;
}

std::vector<std::uint8_t> const& narrowPreambleBytes ()
{
  static std::vector<std::uint8_t> const bytes {
    0x55u, 0x55u, 0x55u, 0x55u, 0x33u, 0xccu
  };
  return bytes;
}

std::vector<std::uint8_t> const& narrowSyncBytes ()
{
  static std::vector<std::uint8_t> const bytes {0xd4u, 0x7au, 0xb2u};
  return bytes;
}

void setError (std::string* error, char const* message)
{
  if (error)
    {
      *error = message;
    }
}

int samplesPerSymbol (W500WaveformConfig const& config)
{
  if (config.sampleRate <= 0.0 || config.symbolRate <= 0.0)
    {
      return 0;
    }
  double const exact = config.sampleRate / config.symbolRate;
  int const rounded = static_cast<int> (std::lround (exact));
  if (rounded <= 0 || std::fabs (exact - double (rounded)) > 1.0e-6)
    {
      return 0;
    }
  return rounded;
}

int samplesPerSymbol (W2300WaveformConfig const& config)
{
  if (config.sampleRate <= 0.0 || config.symbolRate <= 0.0)
    {
      return 0;
    }
  double const exact = config.sampleRate / config.symbolRate;
  int const rounded = static_cast<int> (std::lround (exact));
  if (rounded <= 0 || std::fabs (exact - double (rounded)) > 1.0e-6)
    {
      return 0;
    }
  return rounded;
}

int samplesPerSymbol (NarrowWaveformConfig const& config)
{
  if (config.sampleRate <= 0.0 || config.symbolRate <= 0.0)
    {
      return 0;
    }
  double const exact = config.sampleRate / config.symbolRate;
  int const rounded = static_cast<int> (std::lround (exact));
  if (rounded <= 0 || std::fabs (exact - double (rounded)) > 1.0e-6)
    {
      return 0;
    }
  return rounded;
}

bool validateConfig (W500WaveformConfig const& config, std::string* error)
{
  if (samplesPerSymbol (config) <= 0)
    {
      setError (error, "W500 sample rate must be an integer multiple of symbol rate");
      return false;
    }
  if (config.toneSpacingHz <= 0.0 || config.centerFrequencyHz <= 0.0)
    {
      setError (error, "invalid W500 tone configuration");
      return false;
    }
  if (config.gain <= 0.0 || config.gain > 1.0)
    {
      setError (error, "W500 gain must be in the range (0, 1]");
      return false;
    }
  double const lowest = config.centerFrequencyHz - 1.5 * config.toneSpacingHz;
  double const highest = config.centerFrequencyHz + 1.5 * config.toneSpacingHz;
  if (lowest <= 0.0 || highest >= config.sampleRate * 0.5)
    {
      setError (error, "W500 tones must fit below Nyquist");
      return false;
    }
  return true;
}

bool validateConfig (W2300WaveformConfig const& config, std::string* error)
{
  if (samplesPerSymbol (config) <= 0)
    {
      setError (error, "W2300 sample rate must be an integer multiple of symbol rate");
      return false;
    }
  if (config.subcarrierSpacingHz <= 0.0 || config.centerFrequencyHz <= 0.0)
    {
      setError (error, "invalid W2300 subcarrier configuration");
      return false;
    }
  if (config.gain <= 0.0 || config.gain > 1.0)
    {
      setError (error, "W2300 gain must be in the range (0, 1]");
      return false;
    }
  double const lowest = config.centerFrequencyHz - 1.5 * config.subcarrierSpacingHz;
  double const highest = config.centerFrequencyHz + 1.5 * config.subcarrierSpacingHz;
  if (lowest <= 0.0 || highest >= config.sampleRate * 0.5)
    {
      setError (error, "W2300 subcarriers must fit below Nyquist");
      return false;
    }
  return true;
}

bool validateConfig (NarrowWaveformConfig const& config, std::string* error)
{
  if (samplesPerSymbol (config) <= 0)
    {
      setError (error, "NARROW sample rate must be an integer multiple of symbol rate");
      return false;
    }
  if (config.toneSpacingHz <= 0.0 || config.centerFrequencyHz <= 0.0)
    {
      setError (error, "invalid NARROW tone configuration");
      return false;
    }
  if (config.gain <= 0.0 || config.gain > 1.0)
    {
      setError (error, "NARROW gain must be in the range (0, 1]");
      return false;
    }
  if (config.bitRepetition < 1 || (config.bitRepetition % 2) == 0)
    {
      setError (error, "NARROW bit repetition must be a positive odd number");
      return false;
    }
  double const lowest = config.centerFrequencyHz - 0.5 * config.toneSpacingHz;
  double const highest = config.centerFrequencyHz + 0.5 * config.toneSpacingHz;
  if (lowest <= 0.0 || highest >= config.sampleRate * 0.5)
    {
      setError (error, "NARROW tones must fit below Nyquist");
      return false;
    }
  return true;
}

std::uint8_t dibitToTone (std::uint8_t dibit)
{
  static std::uint8_t const map[4] {0, 1, 3, 2};
  return map[dibit & 0x03u];
}

std::uint8_t toneToDibit (std::uint8_t tone)
{
  static std::uint8_t const map[4] {0, 1, 3, 2};
  return map[tone & 0x03u];
}

void appendLengthSymbols (std::vector<std::uint8_t>& symbols, std::uint16_t length)
{
  for (int shift = 14; shift >= 0; shift -= 2)
    {
      symbols.push_back (dibitToTone (static_cast<std::uint8_t> ((length >> shift) & 0x03u)));
    }
}

void appendByteSymbols (std::vector<std::uint8_t>& symbols, std::uint8_t byte)
{
  for (int shift = 6; shift >= 0; shift -= 2)
    {
      symbols.push_back (dibitToTone (static_cast<std::uint8_t> ((byte >> shift) & 0x03u)));
    }
}

std::uint16_t readLength (std::vector<std::uint8_t> const& symbols, std::size_t offset)
{
  std::uint16_t value = 0;
  for (std::size_t i = 0; i < kLengthSymbols; ++i)
    {
      value = static_cast<std::uint16_t> ((value << 2) | toneToDibit (symbols[offset + i]));
    }
  return value;
}

std::uint8_t readByte (std::vector<std::uint8_t> const& symbols, std::size_t offset)
{
  std::uint8_t value = 0;
  for (std::size_t i = 0; i < 4; ++i)
    {
      value = static_cast<std::uint8_t> ((value << 2) | toneToDibit (symbols[offset + i]));
    }
  return value;
}

int w2300ModeRepetitionFactor (W2300RateMode mode)
{
  switch (mode)
    {
    case W2300RateMode::Fast: return 1;
    case W2300RateMode::Robust: return 3;
    default: return 0;
    }
}

std::uint8_t w2300ModeSymbol (W2300RateMode mode)
{
  switch (mode)
    {
    case W2300RateMode::Fast: return kW2300FastModeSymbol;
    case W2300RateMode::Robust: return kW2300RobustModeSymbol;
    default: return 0;
    }
}

bool w2300ModeFromSymbol (std::uint8_t symbol, W2300RateMode* mode)
{
  if (symbol == kW2300FastModeSymbol)
    {
      if (mode)
        {
          *mode = W2300RateMode::Fast;
        }
      return true;
    }
  if (symbol == kW2300RobustModeSymbol)
    {
      if (mode)
        {
          *mode = W2300RateMode::Robust;
        }
      return true;
    }
  return false;
}

void appendRepeatedByte (std::vector<std::uint8_t>& symbols,
                         std::uint8_t byte,
                         int repetitionFactor)
{
  for (int i = 0; i < repetitionFactor; ++i)
    {
      symbols.push_back (byte);
    }
}

std::uint8_t readRepeatedByte (std::vector<std::uint8_t> const& symbols,
                               std::size_t offset,
                               int repetitionFactor)
{
  if (repetitionFactor <= 1)
    {
      return symbols[offset];
    }

  std::uint8_t value = 0;
  for (int bit = 7; bit >= 0; --bit)
    {
      int ones = 0;
      for (int copy = 0; copy < repetitionFactor; ++copy)
        {
          ones += ((symbols[offset + static_cast<std::size_t> (copy)] >> bit) & 0x01u) != 0u ? 1 : 0;
        }
      if (ones > repetitionFactor / 2)
        {
          value = static_cast<std::uint8_t> (value | (1u << bit));
        }
    }
  return value;
}

double toneFrequency (std::uint8_t symbol, W500WaveformConfig const& config)
{
  return config.centerFrequencyHz + (double (symbol & 0x03u) - 1.5) * config.toneSpacingHz;
}

double subcarrierFrequency (std::size_t carrier, W2300WaveformConfig const& config)
{
  return config.centerFrequencyHz
      + (double (carrier) - 1.5) * config.subcarrierSpacingHz;
}

double narrowToneFrequency (std::uint8_t bit, NarrowWaveformConfig const& config)
{
  return config.centerFrequencyHz
      + ((bit & 0x01u) != 0u ? 0.5 : -0.5) * config.toneSpacingHz;
}

double clamp01 (double value)
{
  return std::max (0.0, std::min (1.0, value));
}

ToneBasis makeToneBasis (int nsps, W500WaveformConfig const& config)
{
  ToneBasis basis;
  basis.i.resize (4);
  basis.q.resize (4);
  for (std::uint8_t symbol = 0; symbol < 4; ++symbol)
    {
      basis.i[symbol].resize (static_cast<std::size_t> (nsps));
      basis.q[symbol].resize (static_cast<std::size_t> (nsps));
      double const omega = 2.0 * kPi * toneFrequency (symbol, config) / config.sampleRate;
      for (int sample = 0; sample < nsps; ++sample)
        {
          double const phase = omega * double (sample);
          basis.i[symbol][static_cast<std::size_t> (sample)] = std::cos (phase);
          basis.q[symbol][static_cast<std::size_t> (sample)] = -std::sin (phase);
        }
    }
  return basis;
}

SubcarrierBasis makeSubcarrierBasis (int nsps, W2300WaveformConfig const& config)
{
  SubcarrierBasis basis;
  basis.i.resize (kW2300Subcarriers);
  basis.q.resize (kW2300Subcarriers);
  for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
    {
      basis.i[carrier].resize (static_cast<std::size_t> (nsps));
      basis.q[carrier].resize (static_cast<std::size_t> (nsps));
      double const omega = 2.0 * kPi * subcarrierFrequency (carrier, config) / config.sampleRate;
      for (int sample = 0; sample < nsps; ++sample)
        {
          double const phase = omega * double (sample);
          basis.i[carrier][static_cast<std::size_t> (sample)] = std::sin (phase);
          basis.q[carrier][static_cast<std::size_t> (sample)] = std::cos (phase);
        }
    }
  return basis;
}

NarrowToneBasis makeNarrowToneBasis (int nsps, NarrowWaveformConfig const& config)
{
  NarrowToneBasis basis;
  basis.i.resize (2);
  basis.q.resize (2);
  for (std::uint8_t bit = 0; bit < 2; ++bit)
    {
      basis.i[bit].resize (static_cast<std::size_t> (nsps));
      basis.q[bit].resize (static_cast<std::size_t> (nsps));
      double const omega = 2.0 * kPi * narrowToneFrequency (bit, config)
          / config.sampleRate;
      for (int sample = 0; sample < nsps; ++sample)
        {
          double const phase = omega * double (sample);
          basis.i[bit][static_cast<std::size_t> (sample)] = std::cos (phase);
          basis.q[bit][static_cast<std::size_t> (sample)] = -std::sin (phase);
        }
    }
  return basis;
}

double edgeEnvelope (std::size_t sample, std::size_t totalSamples, int rampSamples)
{
  if (rampSamples <= 0 || totalSamples == 0)
    {
      return 1.0;
    }
  if (sample < static_cast<std::size_t> (rampSamples))
    {
      return 0.5 * (1.0 - std::cos (kPi * double (sample) / double (rampSamples)));
    }
  std::size_t const tailStart = totalSamples > static_cast<std::size_t> (rampSamples)
      ? totalSamples - static_cast<std::size_t> (rampSamples)
      : 0;
  if (sample >= tailStart)
    {
      return 0.5 * (1.0 + std::cos (kPi * double (sample - tailStart) / double (rampSamples)));
    }
  return 1.0;
}

SymbolDecision detectSymbol (std::vector<float> const& wave,
                             std::size_t start,
                             int nsps,
                             ToneBasis const& basis)
{
  double bestPower = -1.0;
  double secondPower = -1.0;
  std::uint8_t bestSymbol = 0;
  for (std::uint8_t symbol = 0; symbol < 4; ++symbol)
    {
      double iAcc = 0.0;
      double qAcc = 0.0;
      for (int i = 0; i < nsps; ++i)
        {
          double const sample = wave[start + static_cast<std::size_t> (i)];
          iAcc += sample * basis.i[symbol][static_cast<std::size_t> (i)];
          qAcc += sample * basis.q[symbol][static_cast<std::size_t> (i)];
        }
      double const power = iAcc * iAcc + qAcc * qAcc;
      if (power > bestPower)
        {
          secondPower = bestPower;
          bestPower = power;
          bestSymbol = symbol;
        }
      else if (power > secondPower)
        {
          secondPower = power;
        }
    }
  SymbolDecision decision;
  decision.symbol = bestSymbol;
  decision.bestPower = bestPower;
  decision.secondPower = std::max (0.0, secondPower);
  return decision;
}

std::vector<SymbolDecision> detectSymbols (std::vector<float> const& wave,
                                           std::size_t sampleOffset,
                                           int nsps,
                                           ToneBasis const& basis)
{
  std::size_t const symbolCount = (wave.size () - sampleOffset) / static_cast<std::size_t> (nsps);
  std::vector<SymbolDecision> decisions;
  decisions.reserve (symbolCount);
  for (std::size_t sym = 0; sym < symbolCount; ++sym)
    {
      decisions.push_back (detectSymbol (wave,
                                         sampleOffset + sym * static_cast<std::size_t> (nsps),
                                         nsps,
                                         basis));
    }
  return decisions;
}

std::vector<std::uint8_t> decisionSymbols (std::vector<SymbolDecision> const& decisions)
{
  std::vector<std::uint8_t> symbols;
  symbols.reserve (decisions.size ());
  for (SymbolDecision const& decision : decisions)
    {
      symbols.push_back (decision.symbol);
    }
  return symbols;
}

double symbolDecisionQuality (SymbolDecision const& decision)
{
  if (decision.bestPower <= 0.0)
    {
      return 0.0;
    }
  return clamp01 ((decision.bestPower - decision.secondPower) / decision.bestPower);
}

double averageDecisionQuality (std::vector<SymbolDecision> const& decisions,
                               std::size_t start,
                               std::size_t count)
{
  if (count == 0 || decisions.size () < start + count)
    {
      return 0.0;
    }
  double sum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
    {
      sum += symbolDecisionQuality (decisions[start + i]);
    }
  return sum / double (count);
}

bool startsWithSymbols (std::vector<std::uint8_t> const& symbols,
                        std::size_t offset,
                        std::vector<std::uint8_t> const& expected)
{
  if (symbols.size () < offset + expected.size ())
    {
      return false;
    }
  for (std::size_t i = 0; i < expected.size (); ++i)
    {
      if (symbols[offset + i] != expected[i])
        {
          return false;
        }
    }
  return true;
}

// P1 iu8lmc (acquisizione soft, dal banco soglia-dB): distanza di Hamming in
// BIT tra lo stream di simboli e il pattern atteso, con early-exit oltre
// abortAbove. L'acquisizione W2300 a match ESATTO su preamble+sync (24
// simboli-byte = 192 bit non protetti dalla ripetizione) era il collo di
// bottiglia misurato: 1 solo bit errato uccideva l'aggancio — per questo
// ROBUST rendeva quasi come FAST e c'erano fallimenti anche a +18/+24 dB.
// Il match tollerante lascia al CRC (fisico+logico) il rigetto dei falsi.
std::size_t symbolBitErrors (std::vector<std::uint8_t> const& symbols,
                             std::size_t offset,
                             std::vector<std::uint8_t> const& expected,
                             std::size_t abortAbove)
{
  if (symbols.size () < offset + expected.size ())
    {
      return abortAbove + 1u;
    }
  std::size_t errors = 0;
  for (std::size_t i = 0; i < expected.size (); ++i)
    {
      std::uint8_t diff = static_cast<std::uint8_t> (
          symbols[offset + i] ^ expected[i]);
      while (diff != 0u)
        {
          errors += static_cast<std::size_t> (diff & 0x01u);
          diff = static_cast<std::uint8_t> (diff >> 1);
        }
      if (errors > abortAbove)
        {
          return errors;
        }
    }
  return errors;
}

// Budget errori-bit per l'header W2300 (preamble 16 + sync 8 simboli = 192
// bit): ~10%. Un match casuale ne sbaglierebbe ~96 — margine enorme; i
// candidati borderline vengono comunque validati dal CRC.
constexpr std::size_t kW2300HeaderMaxBitErrors = 20u;

bool w2300HeaderMatchesSoft (std::vector<std::uint8_t> const& symbols,
                             std::size_t start)
{
  std::size_t const preambleErrors = symbolBitErrors (
      symbols, start, w2300PreambleSymbols (), kW2300HeaderMaxBitErrors);
  if (preambleErrors > kW2300HeaderMaxBitErrors)
    {
      return false;
    }
  std::size_t const syncErrors = symbolBitErrors (
      symbols, start + w2300PreambleSymbols ().size (), w2300SyncSymbols (),
      kW2300HeaderMaxBitErrors - preambleErrors);
  return preambleErrors + syncErrors <= kW2300HeaderMaxBitErrors;
}

bool parseW2300BurstHeader (std::vector<std::uint8_t> const& symbols,
                            std::size_t start,
                            W2300BurstInfo* info,
                            std::string* error)
{
  if (!info)
    {
      setError (error, "missing W2300 burst info output");
      return false;
    }
  std::size_t const minimumHeaderSymbols = w2300PreambleSymbols ().size ()
      + w2300SyncSymbols ().size () + kW2300ModeSymbols + kW2300LengthSymbols;
  if (symbols.size () < start + minimumHeaderSymbols)
    {
      setError (error, "W2300 symbol stream is too short");
      return false;
    }
  // P1 iu8lmc: match tollerante (vedi w2300HeaderMatchesSoft) al posto del
  // match esatto — l'header non e' protetto dalla ripetizione e il CRC a
  // valle rigetta i falsi positivi.
  if (!w2300HeaderMatchesSoft (symbols, start))
    {
      setError (error, "W2300 preamble or sync mismatch");
      return false;
    }

  std::size_t const modeOffset = start + w2300PreambleSymbols ().size ()
      + w2300SyncSymbols ().size ();
  W2300RateMode mode;
  std::uint8_t const modeSymbol = readRepeatedByte (symbols, modeOffset,
                                                    static_cast<int> (kW2300ModeSymbols));
  if (!w2300ModeFromSymbol (modeSymbol, &mode))
    {
      setError (error, "W2300 rate mode mismatch");
      return false;
    }

  int const repetitionFactor = w2300ModeRepetitionFactor (mode);
  if (repetitionFactor <= 0)
    {
      setError (error, "unsupported W2300 rate mode");
      return false;
    }

  std::size_t const lengthOffset = modeOffset + kW2300ModeSymbols;
  std::size_t const lengthSymbols = kW2300LengthSymbols * static_cast<std::size_t> (repetitionFactor);
  if (symbols.size () < lengthOffset + lengthSymbols)
    {
      setError (error, "W2300 symbol stream is truncated");
      return false;
    }
  std::uint16_t const length = static_cast<std::uint16_t> (
      (static_cast<std::uint16_t> (readRepeatedByte (symbols, lengthOffset, repetitionFactor)) << 8)
      | static_cast<std::uint16_t> (readRepeatedByte (
          symbols,
          lengthOffset + static_cast<std::size_t> (repetitionFactor),
          repetitionFactor)));

  std::size_t const payloadOffset = lengthOffset + lengthSymbols;
  std::size_t const requiredSymbols = payloadOffset - start
      + static_cast<std::size_t> (length) * static_cast<std::size_t> (repetitionFactor);
  if (symbols.size () < start + requiredSymbols)
    {
      setError (error, "W2300 symbol stream is truncated");
      return false;
    }

  info->mode = mode;
  info->repetitionFactor = repetitionFactor;
  info->packetLength = length;
  info->payloadOffset = payloadOffset;
  info->requiredSymbols = requiredSymbols;
  return true;
}

DecodeCandidate extractPacketFromDecisions (std::vector<SymbolDecision> const& decisions,
                                            std::size_t samplePhase,
                                            int nsps,
                                            std::string* error)
{
  DecodeCandidate best;
  std::vector<std::uint8_t> const symbols = decisionSymbols (decisions);
  std::size_t const headerSymbols = preambleSymbols ().size () + syncSymbols ().size () + kLengthSymbols;
  if (symbols.size () < headerSymbols)
    {
      return best;
    }

  for (std::size_t start = 0; start + headerSymbols <= symbols.size (); ++start)
    {
      if (!startsWithSymbols (symbols, start, preambleSymbols ())
          || !startsWithSymbols (symbols, start + preambleSymbols ().size (), syncSymbols ()))
        {
          continue;
        }

      std::uint16_t const length = readLength (symbols, start + preambleSymbols ().size ()
                                               + syncSymbols ().size ());
      std::size_t const requiredSymbols = headerSymbols + static_cast<std::size_t> (length) * 4u;
      if (start + requiredSymbols > symbols.size ())
        {
          continue;
        }

      std::vector<std::uint8_t> burstSymbols (
          symbols.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (start),
          symbols.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (start + requiredSymbols));
      std::vector<std::uint8_t> packet;
      if (w500SymbolsToPacket (burstSymbols, &packet, error))
        {
          DecodeCandidate candidate;
          candidate.ok = true;
          candidate.packet = packet;
          candidate.metrics.sampleOffset = samplePhase + start * static_cast<std::size_t> (nsps);
          candidate.metrics.symbolOffset = start;
          candidate.metrics.symbolCount = requiredSymbols;
          candidate.metrics.packetBytes = packet.size ();
          candidate.metrics.quality = averageDecisionQuality (decisions, start, requiredSymbols);
          if (!best.ok || candidate.metrics.quality > best.metrics.quality)
            {
              best = candidate;
            }
        }
    }
  return best;
}

double knownSymbolQuality (std::vector<float> const& wave,
                           std::size_t sampleOffset,
                           std::vector<std::uint8_t> const& expectedSymbols,
                           int nsps,
                           ToneBasis const& basis)
{
  if (expectedSymbols.empty ()
      || wave.size () < sampleOffset + expectedSymbols.size () * static_cast<std::size_t> (nsps))
    {
      return 0.0;
    }

  double sum = 0.0;
  for (std::size_t sym = 0; sym < expectedSymbols.size (); ++sym)
    {
      SymbolDecision const decision = detectSymbol (wave,
          sampleOffset + sym * static_cast<std::size_t> (nsps),
          nsps,
          basis);
      double const perSymbol = decision.symbol == expectedSymbols[sym]
          ? symbolDecisionQuality (decision)
          : 0.0;
      sum += perSymbol;
    }
  return sum / double (expectedSymbols.size ());
}

void estimateFrequencyMetrics (std::vector<float> const& wave,
                               std::vector<std::uint8_t> const& packet,
                               W500WaveformConfig const& config,
                               int nsps,
                               W500DecodeMetrics* metrics)
{
  if (!metrics)
    {
      return;
    }

  std::string ignored;
  std::vector<std::uint8_t> const expectedSymbols = w500PacketToSymbols (packet, &ignored);
  double bestQuality = metrics->quality;
  double bestOffset = 0.0;
  for (int offsetHz = -16; offsetHz <= 16; offsetHz += 2)
    {
      W500WaveformConfig candidateConfig = config;
      candidateConfig.centerFrequencyHz += double (offsetHz);
      std::string error;
      if (!validateConfig (candidateConfig, &error))
        {
          continue;
        }
      ToneBasis const basis = makeToneBasis (nsps, candidateConfig);
      double const quality = knownSymbolQuality (wave, metrics->sampleOffset, expectedSymbols, nsps, basis);
      if (quality > bestQuality)
        {
          bestQuality = quality;
          bestOffset = double (offsetHz);
        }
    }
  metrics->quality = bestQuality;
  metrics->estimatedFrequencyOffsetHz = bestOffset;
  metrics->estimatedCenterFrequencyHz = config.centerFrequencyHz + bestOffset;
}

double phaseDeltaForDibit (std::uint8_t dibit)
{
  switch (dibit & 0x03u)
    {
    case 0u: return 0.0;
    case 1u: return kPi * 0.5;
    case 2u: return -kPi * 0.5;
    default: return kPi;
    }
}

double wrapPi (double value)
{
  while (value <= -kPi)
    {
      value += 2.0 * kPi;
    }
  while (value > kPi)
    {
      value -= 2.0 * kPi;
    }
  return value;
}

std::uint8_t angleToDibit (double angle, double* quality)
{
  double const candidatePhases[4] {
    0.0,
    kPi * 0.5,
    -kPi * 0.5,
    kPi
  };
  double bestDistance = 10.0;
  double secondDistance = 10.0;
  std::uint8_t best = 0;
  for (std::uint8_t dibit = 0; dibit < 4; ++dibit)
    {
      double const distance = std::fabs (wrapPi (angle - candidatePhases[dibit]));
      if (distance < bestDistance)
        {
          secondDistance = bestDistance;
          bestDistance = distance;
          best = dibit;
        }
      else if (distance < secondDistance)
        {
          secondDistance = distance;
        }
    }
  if (quality)
    {
      *quality = clamp01 ((secondDistance - bestDistance) / (kPi * 0.5));
    }
  return best;
}

W2300SymbolState detectW2300SymbolState (std::vector<float> const& wave,
                                         std::size_t start,
                                         int nsps,
                                         SubcarrierBasis const& basis)
{
  W2300SymbolState state;
  for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
    {
      double iAcc = 0.0;
      double qAcc = 0.0;
      for (int i = 0; i < nsps; ++i)
        {
          double const sample = wave[start + static_cast<std::size_t> (i)];
          iAcc += sample * basis.i[carrier][static_cast<std::size_t> (i)];
          qAcc += sample * basis.q[carrier][static_cast<std::size_t> (i)];
        }
      state.i[carrier] = iAcc;
      state.q[carrier] = qAcc;
    }
  return state;
}

std::vector<W2300SymbolState> detectW2300SymbolStates (std::vector<float> const& wave,
                                                       std::size_t sampleOffset,
                                                       int nsps,
                                                       SubcarrierBasis const& basis)
{
  std::size_t const symbolCount = (wave.size () - sampleOffset) / static_cast<std::size_t> (nsps);
  std::vector<W2300SymbolState> states;
  states.reserve (symbolCount);
  for (std::size_t sym = 0; sym < symbolCount; ++sym)
    {
      states.push_back (detectW2300SymbolState (
          wave,
          sampleOffset + sym * static_cast<std::size_t> (nsps),
          nsps,
          basis));
    }
  return states;
}

std::vector<W2300SymbolDecision> differentialW2300Decisions (
    std::vector<W2300SymbolState> const& states)
{
  std::vector<W2300SymbolDecision> decisions;
  if (states.size () < 2u)
    {
      return decisions;
    }
  decisions.reserve (states.size () - 1u);

  for (std::size_t sym = 1; sym < states.size (); ++sym)
    {
      std::uint8_t byte = 0;
      double qualitySum = 0.0;
      for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
        {
          double const iNow = states[sym].i[carrier];
          double const qNow = states[sym].q[carrier];
          double const iPrev = states[sym - 1u].i[carrier];
          double const qPrev = states[sym - 1u].q[carrier];
          double const dot = iNow * iPrev + qNow * qPrev;
          double const cross = qNow * iPrev - iNow * qPrev;
          double const magNow = std::sqrt (iNow * iNow + qNow * qNow);
          double const magPrev = std::sqrt (iPrev * iPrev + qPrev * qPrev);
          double angularQuality = 0.0;
          std::uint8_t const dibit = angleToDibit (std::atan2 (cross, dot), &angularQuality);
          double magnitudeQuality = 0.0;
          if (magNow > 0.0 && magPrev > 0.0)
            {
              double const normalizedDot = std::sqrt (dot * dot + cross * cross) / (magNow * magPrev);
              magnitudeQuality = clamp01 (normalizedDot);
            }
          byte = static_cast<std::uint8_t> ((byte << 2) | dibit);
          qualitySum += angularQuality * magnitudeQuality;
        }
      W2300SymbolDecision decision;
      decision.symbol = byte;
      decision.quality = qualitySum / double (kW2300Subcarriers);
      decisions.push_back (decision);
    }

  return decisions;
}

std::vector<std::uint8_t> w2300DecisionSymbols (
    std::vector<W2300SymbolDecision> const& decisions)
{
  std::vector<std::uint8_t> symbols;
  symbols.reserve (decisions.size ());
  for (W2300SymbolDecision const& decision : decisions)
    {
      symbols.push_back (decision.symbol);
    }
  return symbols;
}

double averageW2300DecisionQuality (std::vector<W2300SymbolDecision> const& decisions,
                                    std::size_t start,
                                    std::size_t count)
{
  if (count == 0 || decisions.size () < start + count)
    {
      return 0.0;
    }
  double sum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
    {
      sum += decisions[start + i].quality;
    }
  return sum / double (count);
}

W2300DecodeCandidate extractW2300PacketFromDecisions (
    std::vector<W2300SymbolDecision> const& decisions,
    std::size_t samplePhase,
    int nsps,
    std::string* error)
{
  W2300DecodeCandidate best;
  std::vector<std::uint8_t> const symbols = w2300DecisionSymbols (decisions);
  std::size_t const minimumHeaderSymbols = w2300PreambleSymbols ().size ()
      + w2300SyncSymbols ().size () + kW2300ModeSymbols + kW2300LengthSymbols;
  if (symbols.size () < minimumHeaderSymbols)
    {
      return best;
    }

  for (std::size_t start = 0; start + minimumHeaderSymbols <= symbols.size (); ++start)
    {
      // P1 iu8lmc: filtro di aggancio tollerante (Hamming sui 192 bit
      // dell'header) al posto del match esatto; early-exit interno tiene lo
      // scan economico e il CRC a valle elimina i falsi.
      if (!w2300HeaderMatchesSoft (symbols, start))
        {
          continue;
        }

      W2300BurstInfo info;
      if (!parseW2300BurstHeader (symbols, start, &info, error))
        {
          continue;
        }

      std::vector<std::uint8_t> burstSymbols (
          symbols.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (start),
          symbols.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (start + info.requiredSymbols));
      std::vector<std::uint8_t> packet;
      if (w2300SymbolsToPacket (burstSymbols, &packet, error))
        {
          W2300DecodeCandidate candidate;
          candidate.ok = true;
          candidate.packet = packet;
          candidate.metrics.sampleOffset = samplePhase + start * static_cast<std::size_t> (nsps);
          candidate.metrics.symbolOffset = start;
          candidate.metrics.symbolCount = info.requiredSymbols + 1u;
          candidate.metrics.packetBytes = packet.size ();
          candidate.metrics.quality = averageW2300DecisionQuality (decisions, start, info.requiredSymbols);
          candidate.metrics.rateMode = info.mode;
          candidate.metrics.repetitionFactor = info.repetitionFactor;
          if (!best.ok || candidate.metrics.quality > best.metrics.quality)
            {
              best = candidate;
            }
        }
    }

  return best;
}

void estimateW2300FrequencyMetrics (std::vector<float> const& wave,
                                    std::vector<std::uint8_t> const& packet,
                                    W2300WaveformConfig const& config,
                                    int nsps,
                                    W2300DecodeMetrics* metrics)
{
  if (!metrics)
    {
      return;
    }

  std::string ignored;
  std::vector<std::uint8_t> const expectedSymbols = w2300PacketToSymbols (
      packet, metrics->rateMode, &ignored);
  if (expectedSymbols.empty ()
      || wave.size () < metrics->sampleOffset
          + (expectedSymbols.size () + 1u) * static_cast<std::size_t> (nsps))
    {
      metrics->estimatedFrequencyOffsetHz = 0.0;
      metrics->estimatedCenterFrequencyHz = config.centerFrequencyHz;
      return;
    }

  SubcarrierBasis const basis = makeSubcarrierBasis (nsps, config);
  std::vector<W2300SymbolState> const states = detectW2300SymbolStates (
      wave, metrics->sampleOffset, nsps, basis);
  if (states.size () < expectedSymbols.size () + 1u)
    {
      metrics->estimatedFrequencyOffsetHz = 0.0;
      metrics->estimatedCenterFrequencyHz = config.centerFrequencyHz;
      return;
    }

  double residualSin = 0.0;
  double residualCos = 0.0;
  double weightSum = 0.0;
  for (std::size_t sym = 0; sym < expectedSymbols.size (); ++sym)
    {
      std::uint8_t const byte = expectedSymbols[sym];
      for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
        {
          double const iNow = states[sym + 1u].i[carrier];
          double const qNow = states[sym + 1u].q[carrier];
          double const iPrev = states[sym].i[carrier];
          double const qPrev = states[sym].q[carrier];
          double const dot = iNow * iPrev + qNow * qPrev;
          double const cross = qNow * iPrev - iNow * qPrev;
          double const magNow = std::sqrt (iNow * iNow + qNow * qNow);
          double const magPrev = std::sqrt (iPrev * iPrev + qPrev * qPrev);
          if (magNow <= 0.0 || magPrev <= 0.0)
            {
              continue;
            }

          int const shift = static_cast<int> ((kW2300Subcarriers - 1u - carrier) * 2u);
          std::uint8_t const dibit = static_cast<std::uint8_t> ((byte >> shift) & 0x03u);
          double const measured = std::atan2 (cross, dot);
          double const residual = wrapPi (measured - phaseDeltaForDibit (dibit));
          double const weight = clamp01 (std::sqrt (dot * dot + cross * cross) / (magNow * magPrev));
          residualSin += weight * std::sin (residual);
          residualCos += weight * std::cos (residual);
          weightSum += weight;
        }
    }

  double offset = 0.0;
  if (weightSum > 0.0)
    {
      double const averageResidual = std::atan2 (residualSin, residualCos);
      offset = averageResidual * config.symbolRate / (2.0 * kPi);
    }
  metrics->estimatedFrequencyOffsetHz = offset;
  metrics->estimatedCenterFrequencyHz = config.centerFrequencyHz + offset;
}

std::vector<std::uint8_t> narrowPacketFromFrame (Frame const& frame,
                                                 std::string* error)
{
  std::vector<std::uint8_t> const wire = serializeFrame (frame);
  if (wire.empty ())
    {
      setError (error, "NARROW frame serialization failed");
      return {};
    }
  if (wire.size () > 255u)
    {
      setError (error, "NARROW serialized frame is too large");
      return {};
    }

  std::vector<std::uint8_t> packet;
  packet.reserve (4u + wire.size () + 2u);
  packet.push_back (kNarrowPacketMagic0);
  packet.push_back (kNarrowPacketMagic1);
  packet.push_back (kNarrowPacketVersion);
  packet.push_back (static_cast<std::uint8_t> (wire.size ()));
  packet.insert (packet.end (), wire.begin (), wire.end ());
  std::uint16_t const crc = crc16Ccitt (packet);
  packet.push_back (static_cast<std::uint8_t> ((crc >> 8) & 0xffu));
  packet.push_back (static_cast<std::uint8_t> (crc & 0xffu));
  return packet;
}

bool narrowPacketToFrame (std::vector<std::uint8_t> const& packet,
                          Frame* frame,
                          std::string* error)
{
  if (!frame)
    {
      setError (error, "missing NARROW frame output");
      return false;
    }
  if (packet.size () < 6u)
    {
      setError (error, "NARROW packet is too short");
      return false;
    }
  if (packet[0] != kNarrowPacketMagic0 || packet[1] != kNarrowPacketMagic1)
    {
      setError (error, "invalid NARROW packet magic");
      return false;
    }
  if (packet[2] != kNarrowPacketVersion)
    {
      setError (error, "unsupported NARROW packet version");
      return false;
    }
  std::size_t const wireBytes = packet[3];
  if (packet.size () != 4u + wireBytes + 2u)
    {
      setError (error, "NARROW packet length mismatch");
      return false;
    }

  std::vector<std::uint8_t> crcInput (
      packet.begin (),
      packet.end () - static_cast<std::vector<std::uint8_t>::difference_type> (2));
  std::uint16_t const expected = crc16Ccitt (crcInput);
  std::uint16_t const actual = static_cast<std::uint16_t> (
      (static_cast<std::uint16_t> (packet[packet.size () - 2u]) << 8)
      | static_cast<std::uint16_t> (packet[packet.size () - 1u]));
  if (expected != actual)
    {
      setError (error, "NARROW packet CRC mismatch");
      return false;
    }

  std::vector<std::uint8_t> wire (
      packet.begin () + 4,
      packet.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (4u + wireBytes));
  return parseFrame (wire, frame, error);
}

std::vector<std::uint8_t> narrowBurstBytesFromPacket (
    std::vector<std::uint8_t> const& packet)
{
  std::vector<std::uint8_t> bytes;
  bytes.reserve (narrowPreambleBytes ().size () + narrowSyncBytes ().size ()
                 + packet.size ());
  bytes.insert (bytes.end (), narrowPreambleBytes ().begin (),
                narrowPreambleBytes ().end ());
  bytes.insert (bytes.end (), narrowSyncBytes ().begin (),
                narrowSyncBytes ().end ());
  bytes.insert (bytes.end (), packet.begin (), packet.end ());
  return bytes;
}

std::vector<std::uint8_t> narrowRepeatedBitsFromBytes (
    std::vector<std::uint8_t> const& bytes,
    int repetition)
{
  std::vector<std::uint8_t> bits;
  bits.reserve (bytes.size () * 8u * static_cast<std::size_t> (repetition));
  for (std::uint8_t byte : bytes)
    {
      for (int bit = 7; bit >= 0; --bit)
        {
          std::uint8_t const value = static_cast<std::uint8_t> ((byte >> bit) & 0x01u);
          for (int copy = 0; copy < repetition; ++copy)
            {
              bits.push_back (value);
            }
        }
    }
  return bits;
}

NarrowSymbolDecision detectNarrowSymbol (std::vector<float> const& wave,
                                         std::size_t start,
                                         int nsps,
                                         NarrowToneBasis const& basis)
{
  double bestPower = -1.0;
  double secondPower = -1.0;
  std::uint8_t bestBit = 0;
  for (std::uint8_t bit = 0; bit < 2; ++bit)
    {
      double iAcc = 0.0;
      double qAcc = 0.0;
      for (int i = 0; i < nsps; ++i)
        {
          double const sample = wave[start + static_cast<std::size_t> (i)];
          iAcc += sample * basis.i[bit][static_cast<std::size_t> (i)];
          qAcc += sample * basis.q[bit][static_cast<std::size_t> (i)];
        }
      double const power = iAcc * iAcc + qAcc * qAcc;
      if (power > bestPower)
        {
          secondPower = bestPower;
          bestPower = power;
          bestBit = bit;
        }
      else if (power > secondPower)
        {
          secondPower = power;
        }
    }

  NarrowSymbolDecision decision;
  decision.bit = bestBit;
  decision.bestPower = bestPower;
  decision.secondPower = std::max (0.0, secondPower);
  return decision;
}

std::vector<NarrowSymbolDecision> detectNarrowSymbols (
    std::vector<float> const& wave,
    std::size_t sampleOffset,
    int nsps,
    NarrowToneBasis const& basis)
{
  std::size_t const symbolCount = (wave.size () - sampleOffset)
      / static_cast<std::size_t> (nsps);
  std::vector<NarrowSymbolDecision> decisions;
  decisions.reserve (symbolCount);
  for (std::size_t sym = 0; sym < symbolCount; ++sym)
    {
      decisions.push_back (detectNarrowSymbol (
          wave,
          sampleOffset + sym * static_cast<std::size_t> (nsps),
          nsps,
          basis));
    }
  return decisions;
}

double narrowDecisionQuality (NarrowSymbolDecision const& decision)
{
  if (decision.bestPower <= 0.0)
    {
      return 0.0;
    }
  return clamp01 ((decision.bestPower - decision.secondPower) / decision.bestPower);
}

double averageNarrowDecisionQuality (
    std::vector<NarrowSymbolDecision> const& decisions,
    std::size_t start,
    std::size_t count)
{
  if (count == 0 || decisions.size () < start + count)
    {
      return 0.0;
    }
  double sum = 0.0;
  for (std::size_t i = 0; i < count; ++i)
    {
      sum += narrowDecisionQuality (decisions[start + i]);
    }
  return sum / double (count);
}

std::vector<std::uint8_t> narrowMajorityBits (
    std::vector<NarrowSymbolDecision> const& decisions,
    int repetition)
{
  std::vector<std::uint8_t> bits;
  if (repetition <= 0)
    {
      return bits;
    }
  std::size_t const bitCount = decisions.size ()
      / static_cast<std::size_t> (repetition);
  bits.reserve (bitCount);
  for (std::size_t bit = 0; bit < bitCount; ++bit)
    {
      int ones = 0;
      std::size_t const start = bit * static_cast<std::size_t> (repetition);
      for (int copy = 0; copy < repetition; ++copy)
        {
          ones += decisions[start + static_cast<std::size_t> (copy)].bit != 0u ? 1 : 0;
        }
      bits.push_back (ones > repetition / 2 ? 1u : 0u);
    }
  return bits;
}

std::vector<std::uint8_t> narrowBytesFromBits (
    std::vector<std::uint8_t> const& bits)
{
  std::vector<std::uint8_t> bytes;
  bytes.reserve (bits.size () / 8u);
  for (std::size_t offset = 0; offset + 8u <= bits.size (); offset += 8u)
    {
      std::uint8_t value = 0;
      for (std::size_t bit = 0; bit < 8u; ++bit)
        {
          value = static_cast<std::uint8_t> (
              (value << 1) | (bits[offset + bit] & 0x01u));
        }
      bytes.push_back (value);
    }
  return bytes;
}

bool startsWithBytes (std::vector<std::uint8_t> const& bytes,
                      std::size_t offset,
                      std::vector<std::uint8_t> const& expected)
{
  if (bytes.size () < offset + expected.size ())
    {
      return false;
    }
  for (std::size_t i = 0; i < expected.size (); ++i)
    {
      if (bytes[offset + i] != expected[i])
        {
          return false;
        }
    }
  return true;
}

NarrowDecodeCandidate extractNarrowFrameFromDecisions (
    std::vector<NarrowSymbolDecision> const& decisions,
    std::size_t samplePhase,
    int nsps,
    int repetition,
    std::string* error)
{
  NarrowDecodeCandidate best;
  if (repetition <= 0)
    {
      return best;
    }
  std::size_t const prefixBytes = narrowPreambleBytes ().size ()
      + narrowSyncBytes ().size ();
  std::size_t const minimumBytes = prefixBytes + 6u;

  // iu8lmc fix P1 (trovato dal banco soglia-dB): il burst puo' iniziare a
  // QUALSIASI simbolo dello stream RX, ma la votazione a gruppi (repetition)
  // e l'impacchettamento bit->byte partivano sempre dall'indice 0: il
  // preambolo diventava visibile nello stream di byte SOLO se il burst era
  // allineato a multipli di repetition*8 simboli — di fatto solo offset 0
  // (waveform pristine dei test). On-air, con storia/rumore nel buffer prima
  // del burst, il preambolo non veniva MAI trovato ("NARROW burst not found").
  // Fix: prova TUTTI gli allineamenti (repetition fasi di voto x 8 fasi bit);
  // il costo extra e' solo lo scan dei byte, il DSP dei simboli e' invariato.
  for (int repOffset = 0; repOffset < repetition; ++repOffset)
    {
      if (static_cast<std::size_t> (repOffset) >= decisions.size ())
        {
          break;
        }
      std::vector<NarrowSymbolDecision> const shiftedDecisions (
          decisions.begin () + repOffset, decisions.end ());
      std::vector<std::uint8_t> const allBits =
          narrowMajorityBits (shiftedDecisions, repetition);
      for (std::size_t bitOffset = 0;
           bitOffset < 8u && bitOffset < allBits.size (); ++bitOffset)
        {
          std::vector<std::uint8_t> const bits (
              allBits.begin ()
                  + static_cast<std::vector<std::uint8_t>::difference_type> (
                      bitOffset),
              allBits.end ());
          std::vector<std::uint8_t> const bytes = narrowBytesFromBits (bits);
          if (bytes.size () < minimumBytes)
            {
              continue;
            }

          for (std::size_t startByte = 0;
               startByte + minimumBytes <= bytes.size (); ++startByte)
            {
              if (!startsWithBytes (bytes, startByte, narrowPreambleBytes ())
                  || !startsWithBytes (bytes,
                                       startByte + narrowPreambleBytes ().size (),
                                       narrowSyncBytes ()))
                {
                  continue;
                }

              std::size_t const packetOffset = startByte + prefixBytes;
              if (bytes.size () < packetOffset + 6u
                  || bytes[packetOffset] != kNarrowPacketMagic0
                  || bytes[packetOffset + 1u] != kNarrowPacketMagic1)
                {
                  continue;
                }
              std::size_t const packetBytes =
                  4u + static_cast<std::size_t> (bytes[packetOffset + 3u]) + 2u;
              std::size_t const requiredBytes = prefixBytes + packetBytes;
              if (startByte + requiredBytes > bytes.size ())
                {
                  continue;
                }

              std::vector<std::uint8_t> packet (
                  bytes.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (packetOffset),
                  bytes.begin () + static_cast<std::vector<std::uint8_t>::difference_type> (packetOffset + packetBytes));
              Frame frame;
              if (narrowPacketToFrame (packet, &frame, error))
                {
                  // Offset assoluto in simboli dello stream ORIGINALE:
                  // fase di voto + (fase bit + byte) convertiti in simboli.
                  std::size_t const startSymbol =
                      static_cast<std::size_t> (repOffset)
                      + (bitOffset + startByte * 8u)
                            * static_cast<std::size_t> (repetition);
                  NarrowDecodeCandidate candidate;
                  candidate.ok = true;
                  candidate.frame = frame;
                  candidate.metrics.sampleOffset = samplePhase
                      + startSymbol * static_cast<std::size_t> (nsps);
                  candidate.metrics.symbolOffset = startSymbol;
                  candidate.metrics.symbolCount = requiredBytes * 8u
                      * static_cast<std::size_t> (repetition);
                  candidate.metrics.packetBytes = packet.size ();
                  candidate.metrics.quality = averageNarrowDecisionQuality (
                      decisions, candidate.metrics.symbolOffset,
                      candidate.metrics.symbolCount);
                  candidate.metrics.bitRepetition = repetition;
                  if (!best.ok
                      || candidate.metrics.quality > best.metrics.quality)
                    {
                      best = candidate;
                    }
                }
            }
        }
    }
  return best;
}
}

std::vector<std::uint8_t> w500PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                               std::string* error)
{
  if (packet.empty ())
    {
      setError (error, "W500 packet is empty");
      return {};
    }
  if (packet.size () > 0xffffu)
    {
      setError (error, "W500 packet is too large");
      return {};
    }

  std::vector<std::uint8_t> symbols;
  symbols.reserve (preambleSymbols ().size () + syncSymbols ().size ()
                   + kLengthSymbols + packet.size () * 4u);
  symbols.insert (symbols.end (), preambleSymbols ().begin (), preambleSymbols ().end ());
  symbols.insert (symbols.end (), syncSymbols ().begin (), syncSymbols ().end ());
  appendLengthSymbols (symbols, static_cast<std::uint16_t> (packet.size ()));
  for (std::uint8_t byte : packet)
    {
      appendByteSymbols (symbols, byte);
    }
  return symbols;
}

bool w500SymbolsToPacket (std::vector<std::uint8_t> const& symbols,
                          std::vector<std::uint8_t>* packet,
                          std::string* error)
{
  if (!packet)
    {
      setError (error, "missing W500 packet output");
      return false;
    }

  std::size_t const payloadOffset = preambleSymbols ().size () + syncSymbols ().size () + kLengthSymbols;
  if (symbols.size () < payloadOffset)
    {
      setError (error, "W500 symbol stream is too short");
      return false;
    }
  if (!startsWithSymbols (symbols, 0, preambleSymbols ())
      || !startsWithSymbols (symbols, preambleSymbols ().size (), syncSymbols ()))
    {
      setError (error, "W500 preamble or sync mismatch");
      return false;
    }

  std::uint16_t const length = readLength (symbols, preambleSymbols ().size () + syncSymbols ().size ());
  std::size_t const requiredSymbols = payloadOffset + static_cast<std::size_t> (length) * 4u;
  if (symbols.size () < requiredSymbols)
    {
      setError (error, "W500 symbol stream is truncated");
      return false;
    }

  packet->clear ();
  packet->reserve (length);
  for (std::size_t i = 0; i < length; ++i)
    {
      packet->push_back (readByte (symbols, payloadOffset + i * 4u));
    }
  return true;
}

std::vector<float> generateW500Waveform (std::vector<std::uint8_t> const& packet,
                                         W500WaveformConfig const& config,
                                         std::string* error)
{
  if (!validateConfig (config, error))
    {
      return {};
    }

  std::vector<std::uint8_t> const symbols = w500PacketToSymbols (packet, error);
  if (symbols.empty ())
    {
      return {};
    }

  int const nsps = samplesPerSymbol (config);
  std::vector<float> wave (symbols.size () * static_cast<std::size_t> (nsps), 0.0f);
  double phase = 0.0;
  int const rampSamples = nsps / 2;
  for (std::size_t sym = 0; sym < symbols.size (); ++sym)
    {
      double const step = 2.0 * kPi * toneFrequency (symbols[sym], config) / config.sampleRate;
      for (int i = 0; i < nsps; ++i)
        {
          std::size_t const index = sym * static_cast<std::size_t> (nsps)
              + static_cast<std::size_t> (i);
          double const env = edgeEnvelope (index, wave.size (), rampSamples);
          wave[index] = static_cast<float> (config.gain * env * std::sin (phase));
          phase += step;
          if (phase > 2.0 * kPi)
            {
              phase = std::fmod (phase, 2.0 * kPi);
            }
        }
    }
  return wave;
}

bool decodeW500Waveform (std::vector<float> const& wave,
                         std::vector<std::uint8_t>* packet,
                         W500WaveformConfig const& config,
                         std::string* error)
{
  return decodeW500WaveformWithMetrics (wave, packet, nullptr, config, error);
}

bool decodeW500WaveformWithMetrics (std::vector<float> const& wave,
                                    std::vector<std::uint8_t>* packet,
                                    W500DecodeMetrics* metrics,
                                    W500WaveformConfig const& config,
                                    std::string* error)
{
  if (!validateConfig (config, error))
    {
      return false;
    }
  int const nsps = samplesPerSymbol (config);
  if (wave.size () < (preambleSymbols ().size () + syncSymbols ().size () + kLengthSymbols)
      * static_cast<std::size_t> (nsps))
    {
      setError (error, "W500 waveform is too short");
      return false;
    }

  DecodeCandidate best;

  auto searchWithConfig = [&] (W500WaveformConfig const& searchConfig) {
    ToneBasis const basis = makeToneBasis (nsps, searchConfig);
    for (std::size_t phase = 0; phase < static_cast<std::size_t> (nsps); ++phase)
      {
        if (wave.size () - phase < (preambleSymbols ().size () + syncSymbols ().size () + kLengthSymbols)
            * static_cast<std::size_t> (nsps))
          {
            break;
          }
        std::vector<SymbolDecision> const decisions = detectSymbols (wave, phase, nsps, basis);
        DecodeCandidate candidate = extractPacketFromDecisions (decisions, phase, nsps, error);
        if (candidate.ok && (!best.ok || candidate.metrics.quality > best.metrics.quality))
          {
            candidate.metrics.estimatedFrequencyOffsetHz = searchConfig.centerFrequencyHz
                - config.centerFrequencyHz;
            candidate.metrics.estimatedCenterFrequencyHz = searchConfig.centerFrequencyHz;
            best = candidate;
          }
      }
  };

  searchWithConfig (config);
  if (!best.ok)
    {
      for (double offset : {4.0, -4.0, 8.0, -8.0, 12.0, -12.0})
        {
          W500WaveformConfig searchConfig = config;
          searchConfig.centerFrequencyHz += offset;
          std::string configError;
          if (!validateConfig (searchConfig, &configError))
            {
              continue;
            }
          searchWithConfig (searchConfig);
          if (best.ok)
            {
              break;
            }
        }
    }

  if (best.ok)
    {
      estimateFrequencyMetrics (wave, best.packet, config, nsps, &best.metrics);
      if (packet)
        {
          *packet = best.packet;
        }
      if (metrics)
        {
          *metrics = best.metrics;
        }
      return true;
    }

  setError (error, "W500 burst not found");
  return false;
}

std::vector<float> generateW500FrameWaveform (Frame const& frame,
                                              W500WaveformConfig const& config,
                                              std::string* error)
{
  if (frame.profile != Profile::Wide500)
    {
      setError (error, "W500 waveform requires a W500 frame");
      return {};
    }
  std::vector<std::uint8_t> packet = encodePhysicalPacket (frame, error);
  if (packet.empty ())
    {
      return {};
    }
  return generateW500Waveform (packet, config, error);
}

bool decodeW500FrameWaveform (std::vector<float> const& wave,
                              Frame* frame,
                              W500WaveformConfig const& config,
                              std::string* error)
{
  return decodeW500FrameWaveformWithMetrics (wave, frame, nullptr, config, error);
}

bool decodeW500FrameWaveformWithMetrics (std::vector<float> const& wave,
                                         Frame* frame,
                                         W500DecodeMetrics* metrics,
                                         W500WaveformConfig const& config,
                                         std::string* error)
{
  std::vector<std::uint8_t> packet;
  if (!decodeW500WaveformWithMetrics (wave, &packet, metrics, config, error))
    {
      return false;
    }
  return decodePhysicalPacket (Profile::Wide500, packet, frame, error);
}

std::vector<float> generateNarrowFrameWaveform (
    Frame const& frame,
    NarrowWaveformConfig const& config,
    std::string* error)
{
  if (frame.profile != Profile::Narrow)
    {
      setError (error, "NARROW waveform requires a NARROW frame");
      return {};
    }
  if (!validateConfig (config, error))
    {
      return {};
    }

  std::vector<std::uint8_t> const packet = narrowPacketFromFrame (frame, error);
  if (packet.empty ())
    {
      return {};
    }
  std::vector<std::uint8_t> const burstBytes = narrowBurstBytesFromPacket (packet);
  std::vector<std::uint8_t> const bits = narrowRepeatedBitsFromBytes (
      burstBytes, config.bitRepetition);
  if (bits.empty ())
    {
      setError (error, "NARROW waveform has no symbols");
      return {};
    }

  int const nsps = samplesPerSymbol (config);
  std::vector<float> wave (
      bits.size () * static_cast<std::size_t> (nsps), 0.0f);
  int const rampSamples = nsps / 2;
  double phase = 0.0;
  for (std::size_t sym = 0; sym < bits.size (); ++sym)
    {
      double const omega = 2.0 * kPi * narrowToneFrequency (bits[sym], config)
          / config.sampleRate;
      for (int i = 0; i < nsps; ++i)
        {
          std::size_t const index = sym * static_cast<std::size_t> (nsps)
              + static_cast<std::size_t> (i);
          double const env = edgeEnvelope (index, wave.size (), rampSamples);
          wave[index] = static_cast<float> (env * config.gain * std::sin (phase));
          phase += omega;
          if (phase > 2.0 * kPi)
            {
              phase -= 2.0 * kPi;
            }
        }
    }
  return wave;
}

bool decodeNarrowFrameWaveform (
    std::vector<float> const& wave,
    Frame* frame,
    NarrowWaveformConfig const& config,
    std::string* error)
{
  return decodeNarrowFrameWaveformWithMetrics (
      wave, frame, nullptr, config, error);
}

bool decodeNarrowFrameWaveformWithMetrics (
    std::vector<float> const& wave,
    Frame* frame,
    NarrowDecodeMetrics* metrics,
    NarrowWaveformConfig const& config,
    std::string* error)
{
  if (!frame)
    {
      setError (error, "missing NARROW frame output");
      return false;
    }
  if (!validateConfig (config, error))
    {
      return false;
    }

  int const nsps = samplesPerSymbol (config);
  std::size_t const minimumBytes = narrowPreambleBytes ().size ()
      + narrowSyncBytes ().size () + 6u;
  std::size_t const minimumSamples = minimumBytes * 8u
      * static_cast<std::size_t> (config.bitRepetition)
      * static_cast<std::size_t> (nsps);
  if (wave.size () < minimumSamples)
    {
      setError (error, "NARROW waveform is too short");
      return false;
    }

  NarrowDecodeCandidate best;
  auto searchWithConfig = [&] (NarrowWaveformConfig const& searchConfig) {
    NarrowToneBasis const basis = makeNarrowToneBasis (nsps, searchConfig);
    std::size_t const phaseStep = std::max<std::size_t> (
        1u, static_cast<std::size_t> (nsps) / 20u);
    for (std::size_t phase = 0; phase < static_cast<std::size_t> (nsps);
         phase += phaseStep)
      {
        if (wave.size () - phase < minimumSamples)
          {
            break;
          }
        std::vector<NarrowSymbolDecision> const decisions =
            detectNarrowSymbols (wave, phase, nsps, basis);
        NarrowDecodeCandidate candidate = extractNarrowFrameFromDecisions (
            decisions, phase, nsps, searchConfig.bitRepetition, error);
        if (candidate.ok && (!best.ok || candidate.metrics.quality > best.metrics.quality))
          {
            candidate.metrics.estimatedFrequencyOffsetHz =
                searchConfig.centerFrequencyHz - config.centerFrequencyHz;
            candidate.metrics.estimatedCenterFrequencyHz =
                searchConfig.centerFrequencyHz;
            best = candidate;
          }
      }
  };

  searchWithConfig (config);
  if (!best.ok)
    {
      for (double offset : {10.0, -10.0, 20.0, -20.0})
        {
          NarrowWaveformConfig searchConfig = config;
          searchConfig.centerFrequencyHz += offset;
          std::string configError;
          if (!validateConfig (searchConfig, &configError))
            {
              continue;
            }
          searchWithConfig (searchConfig);
          if (best.ok)
            {
              break;
            }
        }
    }

  if (!best.ok)
    {
      setError (error, "NARROW burst not found");
      return false;
    }

  best.metrics.rawBitRate = config.symbolRate;
  best.metrics.payloadBitRate =
      config.symbolRate / double (std::max (1, best.metrics.bitRepetition));
  *frame = best.frame;
  if (metrics)
    {
      *metrics = best.metrics;
    }
  return true;
}

std::vector<std::uint8_t> w2300PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                                std::string* error)
{
  return w2300PacketToSymbols (packet, W2300RateMode::Fast, error);
}

std::vector<std::uint8_t> w2300PacketToSymbols (std::vector<std::uint8_t> const& packet,
                                                W2300RateMode mode,
                                                std::string* error)
{
  if (packet.empty ())
    {
      setError (error, "W2300 packet is empty");
      return {};
    }
  if (packet.size () > 0xffffu)
    {
      setError (error, "W2300 packet is too large");
      return {};
    }
  int const repetitionFactor = w2300ModeRepetitionFactor (mode);
  if (repetitionFactor <= 0)
    {
      setError (error, "unsupported W2300 rate mode");
      return {};
    }

  std::vector<std::uint8_t> symbols;
  symbols.reserve (w2300PreambleSymbols ().size () + w2300SyncSymbols ().size ()
                   + kW2300ModeSymbols
                   + kW2300LengthSymbols * static_cast<std::size_t> (repetitionFactor)
                   + packet.size () * static_cast<std::size_t> (repetitionFactor));
  symbols.insert (symbols.end (), w2300PreambleSymbols ().begin (), w2300PreambleSymbols ().end ());
  symbols.insert (symbols.end (), w2300SyncSymbols ().begin (), w2300SyncSymbols ().end ());
  appendRepeatedByte (symbols, w2300ModeSymbol (mode), static_cast<int> (kW2300ModeSymbols));
  std::uint16_t const length = static_cast<std::uint16_t> (packet.size ());
  appendRepeatedByte (symbols, static_cast<std::uint8_t> ((length >> 8) & 0xffu), repetitionFactor);
  appendRepeatedByte (symbols, static_cast<std::uint8_t> (length & 0xffu), repetitionFactor);
  for (std::uint8_t byte : packet)
    {
      appendRepeatedByte (symbols, byte, repetitionFactor);
    }
  return symbols;
}

bool w2300SymbolsToPacket (std::vector<std::uint8_t> const& symbols,
                           std::vector<std::uint8_t>* packet,
                           std::string* error)
{
  if (!packet)
    {
      setError (error, "missing W2300 packet output");
      return false;
    }

  W2300BurstInfo info;
  if (!parseW2300BurstHeader (symbols, 0, &info, error))
    {
      return false;
    }

  packet->clear ();
  packet->reserve (info.packetLength);
  for (std::size_t i = 0; i < info.packetLength; ++i)
    {
      packet->push_back (readRepeatedByte (
          symbols,
          info.payloadOffset + i * static_cast<std::size_t> (info.repetitionFactor),
          info.repetitionFactor));
    }
  return true;
}

char const* w2300RateModeName (W2300RateMode mode)
{
  switch (mode)
    {
    case W2300RateMode::Fast: return "FAST";
    case W2300RateMode::Robust: return "ROBUST";
    default: return "UNKNOWN";
    }
}

int w2300RateModeRepetitionFactor (W2300RateMode mode)
{
  return w2300ModeRepetitionFactor (mode);
}

W2300RateMode recommendedW2300RateMode (W2300DecodeMetrics const& metrics,
                                        unsigned retryCount)
{
  if (retryCount > 0u)
    {
      return W2300RateMode::Robust;
    }
  if (metrics.quality < 0.55 || std::fabs (metrics.estimatedFrequencyOffsetHz) >= 20.0)
    {
      return W2300RateMode::Robust;
    }
  if (metrics.quality > 0.70 && std::fabs (metrics.estimatedFrequencyOffsetHz) < 15.0)
    {
      return W2300RateMode::Fast;
    }
  return metrics.rateMode;
}

W2300RateController::W2300RateController (W2300RateMode initialMode)
  : m_currentMode {initialMode}
{
}

W2300RateMode W2300RateController::currentMode () const
{
  return m_currentMode;
}

void W2300RateController::reset (W2300RateMode mode)
{
  m_currentMode = mode;
}

void W2300RateController::observe (W2300DecodeMetrics const& metrics,
                                   unsigned retryCount)
{
  m_currentMode = recommendedW2300RateMode (metrics, retryCount);
}

W2300WaveformConfig W2300RateController::configForAttempt (
    int attemptCount,
    W2300WaveformConfig const& base) const
{
  W2300WaveformConfig config = base;
  config.rateMode = attemptCount > 1 ? W2300RateMode::Robust : m_currentMode;
  return config;
}

std::vector<float> generateW2300Waveform (std::vector<std::uint8_t> const& packet,
                                          W2300WaveformConfig const& config,
                                          std::string* error)
{
  if (!validateConfig (config, error))
    {
      return {};
    }

  std::vector<std::uint8_t> const symbols = w2300PacketToSymbols (packet, config.rateMode, error);
  if (symbols.empty ())
    {
      return {};
    }

  int const nsps = samplesPerSymbol (config);
  std::size_t const waveformSymbols = symbols.size () + 1u;
  std::vector<float> wave (waveformSymbols * static_cast<std::size_t> (nsps), 0.0f);
  double phaseOffset[kW2300Subcarriers] {0.0, 0.0, 0.0, 0.0};
  double const carrierGain = config.gain / double (kW2300Subcarriers);
  int const rampSamples = nsps / 2;

  for (std::size_t sym = 0; sym < waveformSymbols; ++sym)
    {
      if (sym > 0)
        {
          std::uint8_t const byte = symbols[sym - 1u];
          for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
            {
              int const shift = static_cast<int> ((kW2300Subcarriers - 1u - carrier) * 2u);
              std::uint8_t const dibit = static_cast<std::uint8_t> ((byte >> shift) & 0x03u);
              phaseOffset[carrier] = wrapPi (phaseOffset[carrier] + phaseDeltaForDibit (dibit));
            }
        }

      for (int i = 0; i < nsps; ++i)
        {
          std::size_t const index = sym * static_cast<std::size_t> (nsps)
              + static_cast<std::size_t> (i);
          double sample = 0.0;
          for (std::size_t carrier = 0; carrier < kW2300Subcarriers; ++carrier)
            {
              double const phase = 2.0 * kPi * subcarrierFrequency (carrier, config)
                  * double (index) / config.sampleRate + phaseOffset[carrier];
              sample += carrierGain * std::sin (phase);
            }
          double const env = edgeEnvelope (index, wave.size (), rampSamples);
          wave[index] = static_cast<float> (env * sample);
        }
    }

  return wave;
}

bool decodeW2300Waveform (std::vector<float> const& wave,
                          std::vector<std::uint8_t>* packet,
                          W2300WaveformConfig const& config,
                          std::string* error)
{
  return decodeW2300WaveformWithMetrics (wave, packet, nullptr, config, error);
}

bool decodeW2300WaveformWithMetrics (std::vector<float> const& wave,
                                     std::vector<std::uint8_t>* packet,
                                     W2300DecodeMetrics* metrics,
                                     W2300WaveformConfig const& config,
                                     std::string* error)
{
  if (!validateConfig (config, error))
    {
      return false;
    }
  int const nsps = samplesPerSymbol (config);
  std::size_t const headerSymbols = w2300PreambleSymbols ().size ()
      + w2300SyncSymbols ().size () + kW2300ModeSymbols + kW2300LengthSymbols;
  if (wave.size () < (headerSymbols + 1u) * static_cast<std::size_t> (nsps))
    {
      setError (error, "W2300 waveform is too short");
      return false;
    }

  W2300DecodeCandidate best;

  auto searchWithConfig = [&] (W2300WaveformConfig const& searchConfig) {
    SubcarrierBasis const basis = makeSubcarrierBasis (nsps, searchConfig);
    for (std::size_t phase = 0; phase < static_cast<std::size_t> (nsps); ++phase)
      {
        if (wave.size () - phase < (headerSymbols + 1u) * static_cast<std::size_t> (nsps))
          {
            break;
          }
        std::vector<W2300SymbolState> const states = detectW2300SymbolStates (
            wave, phase, nsps, basis);
        std::vector<W2300SymbolDecision> const decisions = differentialW2300Decisions (states);
        W2300DecodeCandidate candidate = extractW2300PacketFromDecisions (
            decisions, phase, nsps, error);
        if (candidate.ok && (!best.ok || candidate.metrics.quality > best.metrics.quality))
          {
            candidate.metrics.estimatedFrequencyOffsetHz = searchConfig.centerFrequencyHz
                - config.centerFrequencyHz;
            candidate.metrics.estimatedCenterFrequencyHz = searchConfig.centerFrequencyHz;
            best = candidate;
          }
      }
  };

  searchWithConfig (config);
  if (!best.ok)
    {
      for (double offset : {10.0, -10.0, 20.0, -20.0, 30.0, -30.0})
        {
          W2300WaveformConfig searchConfig = config;
          searchConfig.centerFrequencyHz += offset;
          std::string configError;
          if (!validateConfig (searchConfig, &configError))
            {
              continue;
            }
          searchWithConfig (searchConfig);
          if (best.ok)
            {
              break;
            }
        }
    }

  if (best.ok)
    {
      estimateW2300FrequencyMetrics (wave, best.packet, config, nsps, &best.metrics);
      best.metrics.rawBitRate = config.symbolRate * 8.0;
      best.metrics.payloadBitRate = best.metrics.rawBitRate
          / double (std::max (1, best.metrics.repetitionFactor));
      if (packet)
        {
          *packet = best.packet;
        }
      if (metrics)
        {
          *metrics = best.metrics;
        }
      return true;
    }

  setError (error, "W2300 burst not found");
  return false;
}

std::vector<float> generateW2300FrameWaveform (Frame const& frame,
                                               W2300WaveformConfig const& config,
                                               std::string* error)
{
  if (frame.profile != Profile::Wide2300)
    {
      setError (error, "W2300 waveform requires a W2300 frame");
      return {};
    }
  std::vector<std::uint8_t> packet = encodePhysicalPacket (frame, error);
  if (packet.empty ())
    {
      return {};
    }
  return generateW2300Waveform (packet, config, error);
}

bool decodeW2300FrameWaveform (std::vector<float> const& wave,
                               Frame* frame,
                               W2300WaveformConfig const& config,
                               std::string* error)
{
  return decodeW2300FrameWaveformWithMetrics (wave, frame, nullptr, config, error);
}

bool decodeW2300FrameWaveformWithMetrics (std::vector<float> const& wave,
                                          Frame* frame,
                                          W2300DecodeMetrics* metrics,
                                          W2300WaveformConfig const& config,
                                          std::string* error)
{
  std::vector<std::uint8_t> packet;
  if (!decodeW2300WaveformWithMetrics (wave, &packet, metrics, config, error))
    {
      return false;
    }
  return decodePhysicalPacket (Profile::Wide2300, packet, frame, error);
}

}
}
