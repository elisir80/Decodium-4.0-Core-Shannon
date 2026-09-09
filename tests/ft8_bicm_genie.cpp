// ft8_bicm_genie.cpp — quanto c'e' da guadagnare in FT8 con la demodulazione
// iterativa: il braccio "genio".
//
// PERCHE' SOLO IL GENIO. Su FT2 l'anello completo (tests/ft2_bicm_id.cpp) e'
// stato implementato e misurato: +6,3% di decodifiche, mentre il genio dava
// +34,5%. L'iterazione cattura il 18% di quello che c'e', perche' l'estrinseca
// la si ottiene solo da un decodificatore che ha gia' fallito. Prima di
// rifare tutto quel lavoro su FT8 conviene sapere quanto vale il MASSIMO
// teorico raggiungibile qui, dove il divario dell'informazione mutua e' il
// doppio (1,63 dB su terne contro 1,08 di FT2).
//
// IL GENIO IN MAX-LOG. FT8 demodula con max-log su 8 ipotesi di tono, 3 bit
// per simbolo:
//     llr(j) = max_{i: bit_j(i)=1} s2[i] - max_{i: bit_j(i)=0} s2[i]
// Con i due ALTRI bit del simbolo noti, restano esattamente due ipotesi
// compatibili, una per valore di bit_j: la metrica diventa la differenza fra
// quelle due sole. E' il limite superiore di qualunque schema iterativo.
//
// La mappatura e' quella di run_ft8_bitmetrics: ihalf = c/87, k = (c%87)/3,
// ib = c%3, simbolo ks = k+8 nella prima meta' e k+44 nella seconda (i due
// blocchi Costas stanno in mezzo), bit dell'ipotesi = 2-ib, tono = graymap.
//
// Il banco verifica per prima cosa che a priori nulla riproduca la llra di
// produzione: senza quella verifica i numeri non vogliono dire niente.
// (Su FT2 quella verifica ha trovato tre errori veri.)
//
// Uso:
//   ft8_bicm_genie --verifica
//   ft8_bicm_genie --snr-list "-21,-20,-19,-18" --seeds 25
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStringList>
#include <QTextStream>

#include <fftw3.h>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "Detector/fastldpc/ft2_decoder.hpp"

extern "C"
{
  void ftx_sync8_search_stage4_c (float const* dd, int npts, float nfa, float nfb,
                                  float syncmin, float nfqso, int maxcand, int ipass,
                                  int candthin, float* candidates, int* ncand, float* sbase);
  void ftx_ft8_downsample_c (float const* dd, int* newdat, float f0, fftwf_complex* c1);
  void ftx_ft8_a7_search_initial_c (std::complex<float> const* cd0, int np2, float fs2,
                                    float xdt_in, int* ibest_out, float* delfbest_out);
  void ftx_ft8_a7_refine_search_c (std::complex<float> const* cd0, int np2, float fs2,
                                   int ibest_in, int* ibest_out, float* sync_out,
                                   float* xdt_out);
  void ftx_ft8_bitmetrics_capture_c (std::complex<float> const* cd0, int np2, int ibest,
                                     int imetric, float scale, int weak_deep,
                                     int equalize_tone_power,
                                     std::complex<float> const* history_cs,
                                     std::complex<float>* current_cs_out,
                                     float* s8_out, int* nsync_out,
                                     float* llra, float* llrb, float* llrc,
                                     float* llrd, float* llre);
  void ftx_ldpc174_91_tables_c (int* Mn_out, int* Nm_out, int* nrw_out, int* ncw_out);
  int ftx_encode174_91_message77_c (signed char const* message77, signed char* codeword_out);
}

namespace {

using Complex = std::complex<float>;

constexpr int kSampleRate {12000};
constexpr int kFrameSamples {180000};     // 15 s
constexpr int kNsps {1920};
constexpr float kBt {2.0f};
constexpr int kNp2 {3200};
constexpr float kFs2 {200.0f};
constexpr int kCodeword {174};
constexpr int kBits77 {77};
constexpr int kSyms {79};
constexpr float kScale {2.83f};           // kFt8BitMetricScale
constexpr int kMaxCand {300};
constexpr int kSbase {1800};
constexpr int kGraymap[8] = {0, 1, 3, 2, 5, 6, 4, 7};

[[noreturn]] void fail (QString const& m) { throw std::runtime_error {m.toStdString ()}; }

struct Posto { int simbolo; int bit_ipotesi; };

// Dove vive il bit c del codeword: quale simbolo e quale bit dell'ipotesi.
Posto posto_di (int c)
{
  int const ihalf = c / 87;                 // 0 o 1
  int const dentro = c % 87;
  int const k = dentro / 3 + 1;             // 1..29
  int const ib = dentro % 3;
  int const ks = (ihalf == 0) ? (k + 7) : (k + 43);   // 1-based
  return Posto {ks - 1, 2 - ib};            // one[i][ibmax-ib] con ibmax=2
}

// Demodulatore FT8 a un simbolo (ramo nseq=1 di run_ft8_bitmetrics: max-log su
// 8 ipotesi, metrica = |cs| del tono). Con `veri` non nullo fa il GENIO:
// restringe le ipotesi a quelle compatibili con i due altri bit veri del
// simbolo, cioe' esattamente due.
void demodula (Complex const* cs, uint8_t const* veri, float* llr_out)
{
  std::array<float, kCodeword> m {};
  for (int c = 0; c < kCodeword; ++c)
    {
      Posto const p = posto_di (c);
      float s2[8];
      for (int i = 0; i < 8; ++i)
        s2[i] = std::abs (cs[p.simbolo * 8 + kGraymap[i]]);

      // maschera degli altri due bit del simbolo, quando li conosciamo
      int maschera = 0, valore = 0;
      if (veri)
        {
          int const base = c - (2 - p.bit_ipotesi);   // primo bit del simbolo
          for (int ib = 0; ib < 3; ++ib)
            {
              int const bi = 2 - ib;
              if (bi == p.bit_ipotesi) continue;
              maschera |= (1 << bi);
              if (veri[base + ib]) valore |= (1 << bi);
            }
        }

      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 8; ++i)
        {
          if (maschera && ((i & maschera) != valore)) continue;
          if ((i & (1 << p.bit_ipotesi)) != 0) max1 = std::max (max1, s2[i]);
          else max0 = std::max (max0, s2[i]);
        }
      m[static_cast<size_t> (c)] = max1 - max0;
    }

  // normalizebmet_rms_cpp sui 174, poi la scala di produzione
  double s = 0.0;
  for (int i = 0; i < kCodeword; ++i) s += static_cast<double> (m[static_cast<size_t> (i)]) * m[static_cast<size_t> (i)];
  double sigma = std::sqrt (std::max (s / kCodeword, 0.0));
  if (sigma <= 0.0) sigma = 1.0;
  for (int i = 0; i < kCodeword; ++i)
    llr_out[i] = kScale * static_cast<float> (m[static_cast<size_t> (i)] / sigma);
}

// Demodulatore COERENTE per FT8: la fase del canale stimata dai 21 simboli
// Costas (tre gruppi da 7 ai simboli k, k+36, k+72, toni {3,1,4,0,6,5,2}),
// invece di essere ignorata.
//
// Stessa idea della passata coerente gia' portata in Stage7 per FT2, ma qui il
// tetto e' piu' alto: dall'informazione mutua, FT8 a terne sta a 3,96 contro
// 2,69 del limite coerente, cioe' 1,27 dB contro l'1,1 di FT2. E in FT8 la
// prova in aria e' possibile, perche' la propagazione c'e'.
//
// Con la fase nota l'informazione sta nella parte reale: l'ipotesi si pesa
// sulla sua proiezione, che a differenza del modulo puo' essere NEGATIVA --
// ed e' proprio quella l'informazione in piu' (un tono in controfase e' meno
// probabile di uno a energia nulla).
void demodula_coerente_ft8 (Complex const* cs, float* llr_out)
{
  static int const icos7[7] = {3, 1, 4, 0, 6, 5, 2};
  static int const gruppo[3] = {0, 36, 72};

  Complex ancora[3];
  float centro[3];
  for (int g = 0; g < 3; ++g)
    {
      Complex somma {};
      for (int k = 0; k < 7; ++k)
        somma += cs[(gruppo[g] + k) * 8 + icos7[k]];
      float const mag = std::abs (somma);
      ancora[g] = mag > 0.0f ? somma / mag : Complex {1.0f, 0.0f};
      centro[g] = static_cast<float> (gruppo[g]) + 3.0f;
    }

  std::array<float, kCodeword> m {};
  for (int c = 0; c < kCodeword; ++c)
    {
      Posto const p = posto_di (c);
      float const x = static_cast<float> (p.simbolo);
      Complex rif;
      if (x <= centro[0]) rif = ancora[0];
      else if (x >= centro[2]) rif = ancora[2];
      else
        {
          int const g = (x > centro[1]) ? 1 : 0;
          float const t = (x - centro[g]) / (centro[g + 1] - centro[g]);
          rif = ancora[g] * (1.0f - t) + ancora[g + 1] * t;
        }
      float const rmag = std::abs (rif);
      Complex const rot = rmag > 0.0f ? std::conj (rif / rmag) : Complex {1.0f, 0.0f};

      float s2[8];
      for (int i = 0; i < 8; ++i)
        s2[i] = std::real (cs[p.simbolo * 8 + kGraymap[i]] * rot);

      float max1 = -1.0e30f, max0 = -1.0e30f;
      for (int i = 0; i < 8; ++i)
        {
          if ((i & (1 << p.bit_ipotesi)) != 0) max1 = std::max (max1, s2[i]);
          else max0 = std::max (max0, s2[i]);
        }
      m[static_cast<size_t> (c)] = max1 - max0;
    }

  double s = 0.0;
  for (int i = 0; i < kCodeword; ++i) s += static_cast<double> (m[static_cast<size_t> (i)]) * m[static_cast<size_t> (i)];
  double sigma = std::sqrt (std::max (s / kCodeword, 0.0));
  if (sigma <= 0.0) sigma = 1.0;
  for (int i = 0; i < kCodeword; ++i)
    llr_out[i] = kScale * static_cast<float> (m[static_cast<size_t> (i)] / sigma);
}

Code const& codice ()
{
  static Code c = [] {
    std::vector<int> mn (3 * kCodeword), nm (7 * 83), nrw (83);
    int ncw = 0;
    ftx_ldpc174_91_tables_c (mn.data (), nm.data (), nrw.data (), &ncw);
    Code k;
    k.M = 83; k.N = kCodeword;
    k.row_ptr.push_back (0);
    for (int r = 0; r < k.M; ++r)
      {
        for (int j = 0; j < nrw[static_cast<size_t> (r)]; ++j)
          k.col_idx.push_back (nm[static_cast<size_t> (j + 7 * r)] - 1);
        k.row_ptr.push_back (static_cast<int> (k.col_idx.size ()));
      }
    return k;
  }();
  return c;
}

// FT8 non mescola: la parola vera e' l'encode diretto dei 77 bit.
std::array<uint8_t, kCodeword> codeword_vero (QByteArray const& bits77)
{
  std::array<signed char, kBits77> msg {};
  for (int i = 0; i < kBits77; ++i) msg[static_cast<size_t> (i)] = static_cast<signed char> (bits77.at (i) != 0 ? 1 : 0);
  std::array<signed char, kCodeword> cw {};
  if (ftx_encode174_91_message77_c (msg.data (), cw.data ()) == 0)
    fail (QStringLiteral ("encode174_91 fallito"));
  std::array<uint8_t, kCodeword> out {};
  for (int i = 0; i < kCodeword; ++i) out[static_cast<size_t> (i)] = static_cast<uint8_t> (cw[static_cast<size_t> (i)] & 1);
  return out;
}

Ft2Config produzione_ft8 ()
{
  Ft2Config c = Ft2Decoder::conservativo ();
  c.osd_order = 2; c.span2 = 32; c.span3 = 0;
  c.pair_search = true; c.ntau = 14;
  c.nd_max = 0.065f;
  c.llr_clip = 2.5f;
  c.alpha_w = 37888;
  c.max_iter = 10;
  c.batch = 16;
  c.tipi_ammessi = plaus::kTuttiDefiniti;   // FT8: tutti i tipi definiti
  c.descramble77 = nullptr;                 // FT8 non mescola
  return c;
}

struct Cornice
{
  std::array<Complex, kSyms * 8> cs {};
  std::array<float, kCodeword> llra_prod {};
  bool ok {false};
};

Cornice prepara (QString const& messaggio, float freq, float dt_s, double snr_db, unsigned seme)
{
  Cornice out;
  decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt8 (messaggio);
  if (!enc.ok || enc.tones.isEmpty ()) return out;
  QVector<float> const wave = decodium::txwave::generateFt8Wave (
      enc.tones.constData (), enc.tones.size (), kNsps, kBt, static_cast<float> (kSampleRate), freq);
  if (wave.isEmpty ()) return out;

  int const off = static_cast<int> (std::lround (static_cast<double> (dt_s) * kSampleRate));
  std::vector<float> dd (static_cast<size_t> (kFrameSamples), 0.0f);
  for (int i = 0; i < wave.size () && off + i < kFrameSamples; ++i)
    dd[static_cast<size_t> (off + i)] = 0.5f * wave[i];

  double s2 = 0.0;
  for (float x : dd) s2 += static_cast<double> (x) * x;
  double const rms = std::sqrt (s2 / kFrameSamples);
  double const sigma = rms / std::pow (10.0, snr_db / 20.0);
  std::mt19937 rng {seme};
  std::normal_distribution<float> noise {0.0f, static_cast<float> (sigma)};
  for (float& x : dd) x += noise (rng);

  std::array<float, 4 * kMaxCand> cand {};
  std::array<float, kSbase> sbase {};
  int ncand = 0;
  ftx_sync8_search_stage4_c (dd.data (), kFrameSamples, 200.0f, 3000.0f, 1.2f, freq,
                             kMaxCand, 1, 100, cand.data (), &ncand, sbase.data ());
  if (ncand <= 0) return out;

  // il candidato piu' vicino alla frequenza vera
  int best = -1; float bestd = 1.0e30f;
  for (int i = 0; i < ncand; ++i)
    {
      float const d = std::fabs (cand[static_cast<size_t> (i * 4)] - freq);
      if (d < bestd) { bestd = d; best = i; }
    }
  if (best < 0 || bestd > 20.0f) return out;

  std::array<Complex, kNp2> cd0 {};
  int newdat = 1;
  float f1 = cand[static_cast<size_t> (best * 4)];
  float xdt = cand[static_cast<size_t> (best * 4 + 1)];
  ftx_ft8_downsample_c (dd.data (), &newdat, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));
  int ibest = 0; float delf = 0.0f;
  ftx_ft8_a7_search_initial_c (cd0.data (), kNp2, kFs2, xdt, &ibest, &delf);
  f1 += delf;
  int newdat2 = 0;
  ftx_ft8_downsample_c (dd.data (), &newdat2, f1, reinterpret_cast<fftwf_complex*> (cd0.data ()));
  float sy = 0.0f;
  ftx_ft8_a7_refine_search_c (cd0.data (), kNp2, kFs2, ibest, &ibest, &sy, &xdt);

  std::array<float, 8 * kSyms> s8 {};
  std::array<float, kCodeword> llrb {}, llrc {}, llrd {}, llre {};
  int nsync = 0;
  ftx_ft8_bitmetrics_capture_c (cd0.data (), kNp2, ibest, 0, kScale, 0, 0,
                                nullptr, out.cs.data (), s8.data (), &nsync,
                                out.llra_prod.data (), llrb.data (), llrc.data (),
                                llrd.data (), llre.data ());
  out.ok = true;
  return out;
}

QList<double> lista (QString const& raw)
{
  QList<double> v;
  for (QString p : raw.split (QLatin1Char {','}, Qt::SkipEmptyParts))
    {
      bool ok = false;
      double const x = p.trimmed ().toDouble (&ok);
      if (ok) v.append (x);
    }
  return v;
}

}  // namespace

int main (int argc, char* argv[])
{
  try
    {
      QCoreApplication app {argc, argv};
      QCoreApplication::setApplicationName (QStringLiteral ("ft8_bicm_genie"));
      QCommandLineParser parser;
      parser.setApplicationDescription (
          QStringLiteral ("Limite superiore della demodulazione iterativa in FT8 (braccio genio)."));
      parser.addHelpOption ();
      QCommandLineOption ver_opt {"verifica", "Controlla che a priori nulla riproduca la llra di produzione."};
      QCommandLineOption msg_opt {"message", "Messaggio FT8 (ripetibile).", "text"};
      QCommandLineOption snr_opt {"snr-list", "SNR in dB.", "list", "-22,-21,-20,-19"};
      QCommandLineOption seeds_opt {"seeds", "Semi per punto.", "n", "25"};
      QCommandLineOption freq_opt {"freq", "Frequenza audio.", "hz", "1500.0"};
      QCommandLineOption dt_opt {"dt", "Ritardo del burst in s.", "s", "0.5"};
      parser.addOption (ver_opt); parser.addOption (msg_opt); parser.addOption (snr_opt);
      parser.addOption (seeds_opt); parser.addOption (freq_opt); parser.addOption (dt_opt);
      parser.process (app);

      QStringList messaggi = parser.values (msg_opt);
      if (messaggi.isEmpty ())
        messaggi = {QStringLiteral ("CQ IU8LMC JN70"), QStringLiteral ("IU8LMC DL9XYZ -12"),
                    QStringLiteral ("DL9XYZ IU8LMC R-08"), QStringLiteral ("IU8LMC DL9XYZ 73")};
      bool ok = false;
      int const semi = parser.value (seeds_opt).toInt (&ok);
      float const freq = parser.value (freq_opt).toFloat (&ok);
      float const dt = parser.value (dt_opt).toFloat (&ok);
      QTextStream out {stdout};

      if (parser.isSet (ver_opt))
        {
          Cornice const c = prepara (messaggi.first (), freq, dt, -10.0, 1000000u);
          if (!c.ok) fail (QStringLiteral ("preparazione fallita (nessun candidato?)"));
          std::array<float, kCodeword> mio {};
          demodula (c.cs.data (), nullptr, mio.data ());
          double dmax = 0.0; int peggio = -1;
          for (int i = 0; i < kCodeword; ++i)
            {
              double const d = std::fabs (static_cast<double> (mio[static_cast<size_t> (i)] - c.llra_prod[static_cast<size_t> (i)]));
              if (d > dmax) { dmax = d; peggio = i; }
            }
          out << "scarto massimo " << dmax << " (bit " << peggio << ")\n";
          if (peggio >= 0)
            out << "  mio=" << mio[static_cast<size_t> (peggio)]
                << "  produzione=" << c.llra_prod[static_cast<size_t> (peggio)] << "\n";
          out << (dmax < 1e-3 ? "OK: demodulatore identico a quello di produzione\n"
                              : "DIVERSO: la mappatura non combacia\n");
          return dmax < 1e-3 ? 0 : 1;
        }

      QList<double> const snrs = lista (parser.value (snr_opt));
      out << "semi=" << semi << " messaggi=" << messaggi.size () << "\n\n";
      out << "| SNR | prove | base | genio | coerente | unione |\n|---:|---:|---:|---:|---:|---:|\n";

      Ft2Decoder dec {codice (), produzione_ft8 ()};
      long tp = 0, tb = 0, tg = 0, tc = 0, tu = 0;
      for (double snr : snrs)
        {
          long prove = 0, base = 0, genio = 0, coer = 0, unione = 0;
          for (QString const& m : messaggi)
            {
              decodium::txmsg::EncodedMessage const enc = decodium::txmsg::encodeFt8 (m);
              if (!enc.ok) continue;
              std::array<uint8_t, kCodeword> const vero = codeword_vero (enc.msgbits);
              for (int s = 0; s < semi; ++s)
                {
                  Cornice const c = prepara (m, freq, dt, snr, static_cast<unsigned> (2000000 + s));
                  if (!c.ok) continue;
                  ++prove;
                  std::array<float, kCodeword> l {}, lf {};
                  std::vector<uint8_t> bits (kCodeword), acc (1);
                  auto tenta = [&] (float const* llrd) {
                    for (int i = 0; i < kCodeword; ++i) lf[static_cast<size_t> (i)] = -llrd[i];
                    dec.decode_batch (lf.data (), 1, bits.data (), acc.data ());
                    if (!acc[0]) return false;
                    for (int i = 0; i < kCodeword; ++i)
                      if (bits[static_cast<size_t> (i)] != vero[static_cast<size_t> (i)]) return false;
                    return true;
                  };
                  demodula (c.cs.data (), nullptr, l.data ());
                  bool const ok_base = tenta (l.data ());
                  if (ok_base) ++base;
                  demodula (c.cs.data (), vero.data (), l.data ());
                  if (tenta (l.data ())) ++genio;
                  // ramo coerente REALIZZABILE: fase dai Costas
                  demodula_coerente_ft8 (c.cs.data (), l.data ());
                  bool const ok_coer = tenta (l.data ());
                  if (ok_coer) ++coer;
                  if (ok_base || ok_coer) ++unione;
                }
            }
          out << "| " << snr << " | " << prove << " | " << base << " | " << genio
              << " | " << coer << " | " << unione << " |\n";
          out.flush ();
          tp += prove; tb += base; tg += genio; tc += coer; tu += unione;
        }
      out << "\ntotale su " << tp << " prove: base " << tb << ", genio " << tg
          << ", coerente " << tc << ", unione " << tu << "\n";
      return 0;
    }
  catch (std::exception const& e)
    {
      QTextStream err {stderr};
      err << "ft8_bicm_genie: " << e.what () << '\n';
      return 1;
    }
}
