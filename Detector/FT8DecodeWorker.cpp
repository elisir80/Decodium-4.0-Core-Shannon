// -*- Mode: C++ -*-
#include "Detector/FT8DecodeWorker.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QTimeZone>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/DecodeMetricLogging.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif
extern "C"
{
  void ftx_ft8_stage4_reset_c ();
  void ftx_ft8_stage4_set_cancel_c (int cancel);
  void ftx_ft8_stage4_set_deadline_ms_c (long long deadline_ms);
  void ftx_ft8_stage4_set_ldpc_osd_c (int maxosd, int norder);
  void ftx_ft8_stage4_set_decode_options_c (int low_thresholds, int subpass,
                                            int cycles, int rx_freq_sensitivity,
                                            int candidate_thin);
  void ftx_ft8_stage4_set_supplemental_c (int supplemental);
  void ftx_ft8_stage4_set_force_fresh_slot_c (int force);
  void ftx_ft8_stage4_set_freqpart_c (int bins);
  void ftx_ft8_stage4_set_syncmin_scale_c (float scale);
  void ftx_ft8_stage4_set_decode_syncmin_c (int gate);
  void ftx_ft8_stage4_set_ldpc_max_iter_c (int max_iter);
  void ftx_ft8_stage4_seed_known_cq_c (char const* call, char const* grid,
                                       float freq, float dt, int nutc);
  void ftx_ft8_stage4_seed_known_cq_call_c (char const* call,
                                            float freq, float dt, int nutc);
  void ftx_ft8_stage4_seed_hash_call_c (char const* call);
  void ftx_ft8_stage4_apply_hash_seed_cache_c ();
  void ftx_ft8_a7_save_c (int jseq, float dt, float freq, char const* msg37);
  int ftx_ft8_freqpart_bins_used_c ();
  void ftx_ft8_async_decode_stage4_c (short const* iwave, int* nqsoprogress, int* nfqso, int* nftx,
                                      int* nutc, int* nfa, int* nfb, int* nzhsym, int* ndepth,
                                      float* emedelay, int* ncontest, int* nagain,
                                      int* lft8apon, int* ltry_a8, int* lapcqonly, int* napwid,
                                      char const* mycall, char const* hiscall,
                                      char const* hisgrid, int* ldiskdat, float* syncs, int* snrs,
                                      float* dts, float* freqs, int* naps, float* quals,
                                      signed char* bits77, char* decodeds, int* nout);
}

namespace
{
  constexpr int kFt8SampleCount {180000};
  constexpr int kFt8MaxLines {200};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt8StableDspStage {4};
  constexpr qint64 kHashSeedTailBytes {8 * 1024 * 1024};
  constexpr qint64 kHashSeedRefreshTailBytes {512 * 1024};
  constexpr int kHashSeedMaxCalls {4096};
  constexpr qint64 kHashSeedRefreshIntervalMs {10000};
  constexpr int kExternalA7SeedLimit {96};
  constexpr int kExternalA7PairSeedLimit {48};
  constexpr int kExternalA7InsertLimit {104};
  constexpr int kHashSeedPriorityMonths {2};
  [[maybe_unused]] constexpr int kMaxDecodeThreads {24};

  void apply_decode_thread_limit (int threads)
  {
#ifdef _OPENMP
    omp_set_dynamic (0);
    omp_set_num_threads (std::max (1, std::min (threads, kMaxDecodeThreads)));
#else
    (void) threads;
#endif
  }

  int active_decode_thread_limit ()
  {
#ifdef _OPENMP
    return omp_get_max_threads ();
#else
    return 1;
#endif
  }

  QString current_thread_id_hex ()
  {
    return QString::number (reinterpret_cast<quintptr> (QThread::currentThreadId ()), 16);
  }

  void set_ft8_stage4_cancel (bool cancel)
  {
    ftx_ft8_stage4_set_cancel_c (cancel ? 1 : 0);
  }

  long long ft8_stage4_deadline_ms_from_now (int maxDecodeMs)
  {
    if (maxDecodeMs <= 0)
      {
        return 0;
      }
    using namespace std::chrono;
    auto const now = steady_clock::now ().time_since_epoch ();
    return duration_cast<milliseconds> (now).count () + maxDecodeMs;
  }

  QString format_decode_utc (int nutc)
  {
    if (nutc <= 0)
      {
        return QString {};
      }
    return QString::number (nutc).rightJustified (6, QLatin1Char {'0'});
  }

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      {
        value.append (QByteArray (width - value.size (), ' '));
      }
    return value;
  }

  QString decode_fallback (char const* decodeds, int index)
  {
    QByteArray fallback {decodeds + index * kDecodedChars, kDecodedChars};
    int end = fallback.size ();
    while (end > 0 && (fallback.at (end - 1) == ' ' || fallback.at (end - 1) == '\0'))
      {
        --end;
      }
    QString decoded = QString::fromLatin1 (fallback.constData (), end);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    return decoded.left (kDecodedChars);
  }

  bool plausible_hash_seed_token (QString const& token)
  {
    if (token.size () < 3 || token.size () > 13)
      {
        return false;
      }
    static QSet<QString> const skipTokens {
      QStringLiteral ("CQ"), QStringLiteral ("DE"), QStringLiteral ("DX"),
      QStringLiteral ("QRZ"), QStringLiteral ("RRR"), QStringLiteral ("RR73"),
      QStringLiteral ("FT8"), QStringLiteral ("FT4"), QStringLiteral ("FT2"),
      QStringLiteral ("JT9"), QStringLiteral ("JT65"), QStringLiteral ("Q65"),
      QStringLiteral ("WSPR"), QStringLiteral ("FST4")
    };
    if (skipTokens.contains (token))
      {
        return false;
      }
    static QRegularExpression const reportRx {
      QStringLiteral (R"(^(?:R)?[+-]?\d{1,2}$)")
    };
    static QRegularExpression const modeSuffixRx {QStringLiteral (R"(^A\d+$)")};
    static QRegularExpression const gridRx {
      QStringLiteral (R"(^[A-R]{2}\d{2}(?:[A-X]{2})?$)")
    };
    if (reportRx.match (token).hasMatch ()
        || modeSuffixRx.match (token).hasMatch ()
        || gridRx.match (token).hasMatch ())
      {
        return false;
      }

    bool hasLetter = false;
    bool hasDigit = false;
    for (QChar const ch : token)
      {
        if (ch >= QLatin1Char ('A') && ch <= QLatin1Char ('Z'))
          {
            hasLetter = true;
          }
        else if (ch >= QLatin1Char ('0') && ch <= QLatin1Char ('9'))
          {
            hasDigit = true;
          }
        else if (ch != QLatin1Char ('/'))
          {
            return false;
          }
      }
    return hasLetter && hasDigit;
  }

  void remember_hash_seed_call (QStringList& calls, QString call)
  {
    call = call.trimmed ().toUpper ();
    if (call.startsWith (QLatin1Char ('<')) && call.endsWith (QLatin1Char ('>')))
      {
        call = call.mid (1, call.size () - 2);
      }
    if (!plausible_hash_seed_token (call))
      {
        return;
      }
    calls.removeAll (call);
    calls.append (call);
    while (calls.size () > kHashSeedMaxCalls)
      {
        calls.removeFirst ();
      }
  }

  bool collect_hash_seed_calls_from_tail (QString const& path, QStringList& calls,
                                          qint64 tailBytes = kHashSeedTailBytes)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }
    QByteArray const data = file.readAll ().toUpper ();
    QString const text = QString::fromLatin1 (data.constData (), data.size ());
    static QRegularExpression const tokenRx {
      QStringLiteral (R"((?:<[A-Z0-9/]{3,13}>|[A-Z0-9/]{3,13}))")
    };
    auto it = tokenRx.globalMatch (text);
    while (it.hasNext ())
      {
        remember_hash_seed_call (calls, it.next ().captured (0));
      }
    return true;
  }

  struct KnownCqSeed
  {
    QString call;
    QString grid;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
  };

  struct KnownCqCallSeed
  {
    QString call;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
  };

  struct A7MessageSeed
  {
    QString message;
    float freq {0.0f};
    float dt {0.0f};
    int nutc {0};
    int hits {1};
    int priority {0};
  };

  void remember_known_cq_seed (QHash<QString, KnownCqSeed>& seeds, QString call,
                               QString grid, float freq, float dt, int nutc)
  {
    call = call.trimmed ().toUpper ();
    grid = grid.trimmed ().toUpper ();
    if (!plausible_hash_seed_token (call)
        || !QRegularExpression {QStringLiteral (R"(^[A-R]{2}\d{2}$)")}.match (grid).hasMatch ()
        || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    QString const key = call + QLatin1Char ('|') + grid;
    auto it = seeds.find (key);
    if (it == seeds.end ())
      {
        seeds.insert (key, KnownCqSeed {call, grid, freq, dt, nutc, 1});
        return;
      }
    KnownCqSeed& seed = it.value ();
    seed.freq = freq;
    seed.dt = dt;
    seed.nutc = nutc;
    seed.hits = std::min (seed.hits + 1, 20);
  }

  void remember_known_cq_call_seed (QHash<QString, KnownCqCallSeed>& seeds,
                                    QString call, float freq, float dt, int nutc)
  {
    call = call.trimmed ().toUpper ();
    if (!plausible_hash_seed_token (call) || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    auto it = seeds.find (call);
    if (it == seeds.end ())
      {
        seeds.insert (call, KnownCqCallSeed {call, freq, dt, nutc, 1});
        return;
      }
    KnownCqCallSeed& seed = it.value ();
    seed.freq = freq;
    seed.dt = dt;
    seed.nutc = nutc;
    seed.hits = std::min (seed.hits + 1, 20);
  }

  int ft8_nutc_seconds (int nutc)
  {
    int const raw = std::abs (nutc);
    int const seconds = raw % 100;
    int const minutes = (raw / 100) % 100;
    int const hours = (raw / 10000) % 100;
    return hours * 3600 + minutes * 60 + seconds;
  }

  int ft8_nutc_delta_seconds (int newer, int older)
  {
    int delta = ft8_nutc_seconds (newer) - ft8_nutc_seconds (older);
    if (delta < -12 * 3600)
      {
        delta += 24 * 3600;
      }
    if (delta > 12 * 3600)
      {
        delta -= 24 * 3600;
      }
    return delta;
  }

  bool collect_known_cq_call_seeds_from_tail (QString const& path,
                                              QHash<QString, KnownCqCallSeed>& seeds,
                                              qint64 tailBytes,
                                              int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxCqCallRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+CQ\s+([A-Z0-9/]{3,12})(?:\s+[\^*?#]+)?\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumCqCallRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+CQ\s+([A-Z0-9/]{3,12})(?:\s+[\^*?#]+)?\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxAgeSeconds = 30 * 60 + 15;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxCqCallRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumCqCallRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const age = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (age < 0 || age > kMaxAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0 && ft8_nutc_delta_seconds (currentNutc, nutc) <= 0)
          {
            continue;
          }
        remember_known_cq_call_seed (seeds, match.captured (5),
                                     freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  bool collect_known_cq_seeds_from_tail (QString const& path,
                                         QHash<QString, KnownCqSeed>& seeds,
                                         qint64 tailBytes,
                                         int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxCqRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+CQ\s+([A-Z0-9/]{3,12})\s+([A-R]{2}\d{2})\b)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumCqRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+CQ\s+([A-Z0-9/]{3,12})\s+([A-R]{2}\d{2})\b)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxAgeSeconds = 3 * 3600 + 15;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxCqRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumCqRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const age = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (age < 0 || age > kMaxAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0 && ft8_nutc_delta_seconds (currentNutc, nutc) <= 0)
          {
            continue;
          }
        remember_known_cq_seed (seeds, match.captured (5), match.captured (6),
                                freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  bool plausible_a7_message_token (QString const& token)
  {
    if (token == QStringLiteral ("CQ"))
      {
        return true;
      }
    if (token == QStringLiteral ("RRR") || token == QStringLiteral ("RR73")
        || token == QStringLiteral ("73"))
      {
        return true;
      }
    static QRegularExpression const reportRx {
      QStringLiteral (R"(^R?[+-]?\d{1,2}$)")
    };
    static QRegularExpression const gridRx {
      QStringLiteral (R"(^[A-R]{2}\d{2}$)")
    };
    if (reportRx.match (token).hasMatch () || gridRx.match (token).hasMatch ())
      {
        return true;
      }
    return plausible_hash_seed_token (token);
  }

  QString normalize_a7_logged_message (QString message)
  {
    message = message.trimmed ().toUpper ();
    message.remove (QRegularExpression {QStringLiteral (R"(\s+[\^*?#]+$)")});
    message = message.simplified ();
    if (message.isEmpty () || message.size () > kDecodedChars
        || message.contains (QLatin1Char ('<')) || message.contains (QLatin1Char ('/')))
      {
        return {};
      }

    QStringList const words = message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () < 2 || words.size () > 4)
      {
        return {};
      }
    if (words.first () == QStringLiteral ("CQ"))
      {
        if (words.size () > 3 || !plausible_hash_seed_token (words.value (1)))
          {
            return {};
          }
        if (words.size () == 3
            && !QRegularExpression {QStringLiteral (R"(^[A-R]{2}\d{2}$)")}.match (words.value (2)).hasMatch ())
          {
            return {};
          }
        return message;
      }
    if (!plausible_hash_seed_token (words.value (0))
        || !plausible_hash_seed_token (words.value (1)))
      {
        return {};
      }
    for (int index = 2; index < words.size (); ++index)
      {
        if (!plausible_a7_message_token (words.value (index)))
          {
            return {};
          }
      }
    return message;
  }

  int a7_message_seed_priority (QString const& message)
  {
    QStringList const words =
        message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () >= 3 && words.first () != QStringLiteral ("CQ"))
      {
        return 3;
      }
    if (words.size () >= 3 && words.first () == QStringLiteral ("CQ"))
      {
        return 2;
      }
    return 1;
  }

  void remember_a7_message_seed (QHash<QString, A7MessageSeed>& seeds,
                                 QString message, float freq, float dt, int nutc,
                                 int priority = 0)
  {
    message = normalize_a7_logged_message (message);
    if (message.isEmpty () || freq <= 0.0f || nutc <= 0)
      {
        return;
      }
    if (priority <= 0)
      {
        priority = a7_message_seed_priority (message);
      }
    auto it = seeds.find (message);
    if (it == seeds.end ())
      {
        seeds.insert (message, A7MessageSeed {message, freq, dt, nutc, 1, priority});
        return;
      }
    A7MessageSeed& seed = it.value ();
    seed.hits = std::min (seed.hits + 1, 16);
    seed.priority = std::max (seed.priority, priority);
    if (ft8_nutc_delta_seconds (nutc, seed.nutc) >= 0)
      {
        seed.freq = freq;
        seed.dt = dt;
        seed.nutc = nutc;
      }
  }

  bool a7_seed_is_directed_pair (A7MessageSeed const& seed)
  {
    QStringList const words =
        seed.message.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    return seed.priority >= 4 && words.size () == 2;
  }

  void remember_a7_directed_pair_seeds (QHash<QString, A7MessageSeed>& seeds,
                                        QString const& normalizedMessage,
                                        float freq, float dt, int nutc)
  {
    QStringList const words =
        normalizedMessage.split (QLatin1Char (' '), Qt::SkipEmptyParts);
    if (words.size () < 3 || words.first () == QStringLiteral ("CQ"))
      {
        return;
      }

    QString const call1 = words.value (0);
    QString const call2 = words.value (1);
    if (call1 == call2 || call1.contains (QLatin1Char ('/'))
        || call2.contains (QLatin1Char ('/'))
        || !plausible_hash_seed_token (call1)
        || !plausible_hash_seed_token (call2))
      {
        return;
      }

    remember_a7_message_seed (seeds, call1 + QLatin1Char (' ') + call2,
                              freq, dt, nutc, 4);
    remember_a7_message_seed (seeds, call2 + QLatin1Char (' ') + call1,
                              freq, dt, nutc, 4);
  }

  bool collect_a7_message_seeds_from_tail (QString const& path,
                                           QHash<QString, A7MessageSeed>& seeds,
                                           qint64 tailBytes,
                                           int currentNutc)
  {
    QFile file {path};
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
      {
        return false;
      }
    if (tailBytes > 0 && file.size () > tailBytes)
      {
        file.seek (file.size () - tailBytes);
        file.readLine ();
      }

    static QRegularExpression const jtdxRx {
      QStringLiteral (R"(^(\d{8})_(\d{6})\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+~\s+(.+?)\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };
    static QRegularExpression const decodiumRx {
      QStringLiteral (R"(^(\d{6})_(\d{6})\s+\S+\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.+?)\s*$)"),
      QRegularExpression::CaseInsensitiveOption
    };

    QDateTime const nowUtc = QDateTime::currentDateTimeUtc ();
    constexpr qint64 kMaxWallAgeSeconds = 30 * 60 + 15;
    constexpr int kMaxDecodeAgeSeconds = 75;
    auto lineDate = [] (QString const& token) {
      if (token.size () == 8)
        {
          return QDate (token.left (4).toInt (), token.mid (4, 2).toInt (),
                        token.mid (6, 2).toInt ());
        }
      return QDate (2000 + token.left (2).toInt (), token.mid (2, 2).toInt (),
                    token.mid (4, 2).toInt ());
    };

    while (!file.atEnd ())
      {
        QString const line = QString::fromLatin1 (file.readLine ()).trimmed ().toUpper ();
        QRegularExpressionMatch match = jtdxRx.match (line);
        if (!match.hasMatch ())
          {
            match = decodiumRx.match (line);
          }
        if (!match.hasMatch ())
          {
            continue;
          }

        QDate const date = lineDate (match.captured (1));
        QTime const time = QTime::fromString (match.captured (2), QStringLiteral ("HHmmss"));
        QDateTime const when {date, time, QTimeZone::UTC};
        qint64 const wallAge = when.isValid () ? when.secsTo (nowUtc) : -1;
        if (wallAge < 0 || wallAge > kMaxWallAgeSeconds)
          {
            continue;
          }

        bool okDt = false;
        bool okFreq = false;
        bool okNutc = false;
        float const dt = match.captured (3).toFloat (&okDt);
        float const freq = match.captured (4).toFloat (&okFreq);
        int const nutc = match.captured (2).toInt (&okNutc);
        if (!okFreq || !okNutc)
          {
            continue;
          }
        if (currentNutc > 0)
          {
            int const decodeAge = ft8_nutc_delta_seconds (currentNutc, nutc);
            if (decodeAge <= 0 || decodeAge > kMaxDecodeAgeSeconds)
              {
                continue;
              }
          }
        QString const normalizedMessage = normalize_a7_logged_message (match.captured (5));
        if (!normalizedMessage.startsWith (QStringLiteral ("CQ ")))
          {
            remember_a7_message_seed (seeds, normalizedMessage,
                                      freq, okDt ? dt : 0.0f, nutc);
          }
        remember_a7_directed_pair_seeds (seeds, normalizedMessage,
                                         freq, okDt ? dt : 0.0f, nutc);
      }
    return true;
  }

  QStringList hash_seed_monthly_all_files ()
  {
    QStringList names;
    QDate const currentMonth = QDate::currentDate ();
    for (int i = kHashSeedPriorityMonths - 1; i >= 0; --i)
      {
        QDate const month = currentMonth.addMonths (-i);
        names.append (month.toString (QStringLiteral ("yyyyMM"))
                      + QStringLiteral ("_ALL.TXT"));
      }
    return names;
  }

  QString hash_seed_source_label (QString const& path)
  {
    QFileInfo const info {path};
    QFileInfo const dirInfo {info.absolutePath ()};
    return dirInfo.fileName () + QLatin1Char ('/')
           + info.fileName () + QStringLiteral (":")
           + QString::number (info.size ());
  }

  QStringList local_hash_seed_paths ()
  {
    QStringList paths;
    QSet<QString> seen;
    auto addPath = [&paths, &seen] (QString const& path, bool priority = false) {
      QString const clean = QDir::cleanPath (path);
      if (clean.isEmpty ())
        {
          return;
        }
      if (seen.contains (clean))
        {
          if (priority)
            {
              paths.removeAll (clean);
              paths.append (clean);
            }
          return;
        }
      if (!clean.isEmpty ())
        {
          seen.insert (clean);
          paths.append (clean);
        }
    };

    auto addLogDir = [&addPath] (QString const& path, bool priority = false) {
      QDir const dir {path};
      if (!dir.exists ())
        {
          return;
        }
      static QStringList const filters {
        QStringLiteral ("CALL3.TXT"),
        QStringLiteral ("ALL.TXT"),
        QStringLiteral ("all.txt"),
        QStringLiteral ("*_ALL.TXT")
      };
      for (QString const& name : dir.entryList (filters, QDir::Files, QDir::Name))
        {
          addPath (dir.filePath (name), priority);
        }
    };

    auto addMonthlyAllTxt = [&addPath] (QString const& path, bool priority) {
      QDir const dir {path};
      if (!dir.exists ())
        {
          return;
        }
      for (QString const& name : hash_seed_monthly_all_files ())
        {
          addPath (dir.filePath (name), priority);
        }
    };

    addLogDir (QStandardPaths::writableLocation (QStandardPaths::AppDataLocation));

    QStringList dataRoots;
    auto addDataRoot = [&dataRoots] (QString const& root) {
      QString const clean = QDir::cleanPath (root);
      if (!clean.isEmpty () && !dataRoots.contains (clean))
        {
          dataRoots.append (clean);
        }
    };
    addDataRoot (QStandardPaths::writableLocation (QStandardPaths::GenericDataLocation));
    addDataRoot (QDir {QDir::homePath ()}.filePath (QStringLiteral ("Library/Application Support")));

    for (QString const& root : dataRoots)
      {
        QDir const dir {root};
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium")));
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium/embedded-ft2")));
        addLogDir (dir.filePath (QStringLiteral ("WSJT-X")));
        addLogDir (dir.filePath (QStringLiteral ("JTDX")));
        addLogDir (dir.filePath (QStringLiteral ("IU8LMC/Decodium/embedded-ft2")), true);
        addMonthlyAllTxt (dir.filePath (QStringLiteral ("WSJT-X")), true);
        addMonthlyAllTxt (dir.filePath (QStringLiteral ("JTDX")), true);
      }
    return paths;
  }

  void log_hash_seed_probe (QStringList const& calls)
  {
    QString probesText =
        qEnvironmentVariable (QByteArrayLiteral ("DECODIUM_FT8_HASHDBG_CALLS"));
    if (probesText.trimmed ().isEmpty ())
      {
        return;
      }
    QStringList results;
    static QRegularExpression const splitRx {QStringLiteral (R"([,\s;]+)")};
    for (QString probe : probesText.split (splitRx, Qt::SkipEmptyParts))
      {
        probe = probe.trimmed ().toUpper ();
        if (!probe.isEmpty ())
          {
            results.append (probe + QLatin1Char ('=')
                            + (calls.contains (probe)
                                   ? QStringLiteral ("1")
                                   : QStringLiteral ("0")));
          }
      }
    if (!results.isEmpty ())
      {
        LOG_INFO ("FT8 pack77 hash seed probes: "
                  << results.join (QLatin1Char (' ')).toStdString ());
      }
  }

  void seed_ft8_pack77_hashes_from_local_logs_once ()
  {
    static std::once_flag once;
    std::call_once (once, [] {
      QStringList calls;
      int filesRead = 0;
      QStringList sources;
      for (QString const& path : local_hash_seed_paths ())
        {
          if (collect_hash_seed_calls_from_tail (path, calls))
            {
              ++filesRead;
              sources.append (hash_seed_source_label (path));
            }
        }
      for (QString const& call : calls)
        {
          QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
          ftx_ft8_stage4_seed_hash_call_c (field.constData ());
        }
      if (filesRead > 0 && !calls.isEmpty ())
        {
          LOG_INFO ("FT8 pack77 hash seed initialized: calls=" << calls.size ()
                    << " files=" << filesRead
                    << " sources="
                    << sources.join (QStringLiteral (",")).toStdString ());
          log_hash_seed_probe (calls);
        }
    });
  }

  bool external_known_cq_seed_enabled ()
  {
    QString const value =
        qEnvironmentVariable ("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY").trimmed ().toLower ();
    if (value.isEmpty ())
      {
        return true;
      }
    return value == QStringLiteral ("1")
           || value == QStringLiteral ("true")
           || value == QStringLiteral ("yes");
  }

  bool external_known_cq_source_allowed (QString const& path)
  {
    QFileInfo const info {path};
    QString const dir = info.absolutePath ();
    QString const name = info.fileName ();
    return dir.contains (QStringLiteral ("JTDX"), Qt::CaseInsensitive)
           || dir.contains (QStringLiteral ("WSJT-X"), Qt::CaseInsensitive)
           || dir.contains (QStringLiteral ("Decodium"), Qt::CaseInsensitive)
           || name.startsWith (QStringLiteral ("JTDX"), Qt::CaseInsensitive)
           || name == QStringLiteral ("ALL.TXT")
           || name == QStringLiteral ("all.txt")
           || name.endsWith (QStringLiteral ("_ALL.TXT"), Qt::CaseInsensitive);
  }

  bool stable_recent_known_cq_seed (KnownCqSeed const& seed, int currentNutc)
  {
    if (seed.hits < 4 || currentNutc <= 0)
      {
        return false;
      }
    int const age = ft8_nutc_delta_seconds (currentNutc, seed.nutc);
    return age > 0 && age <= 150 && std::fabs (seed.dt) <= 2.5f;
  }

  bool stable_recent_known_cq_call_seed (KnownCqCallSeed const& seed,
                                         int currentNutc)
  {
    if (seed.hits < 4 || currentNutc <= 0)
      {
        return false;
    }
    int const age = ft8_nutc_delta_seconds (currentNutc, seed.nutc);
    return age > 0 && age <= 150 && std::fabs (seed.dt) <= 2.5f;
  }

  bool external_a7_seed_enabled ()
  {
    QString const value =
        qEnvironmentVariable ("DECODIUM_FT8_SEED_A7_HINTS").trimmed ().toLower ();
    if (value.isEmpty ())
      {
        return true;
      }
    if (value == QStringLiteral ("0")
        || value == QStringLiteral ("false")
        || value == QStringLiteral ("no"))
      {
        return false;
      }
    return value == QStringLiteral ("1")
           || value == QStringLiteral ("true")
           || value == QStringLiteral ("yes");
  }

  void refresh_ft8_hash_context_from_recent_logs (int currentNutc)
  {
    static qint64 lastRefreshMs = 0;
    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch ();
    if (lastRefreshMs > 0 && nowMs - lastRefreshMs < kHashSeedRefreshIntervalMs)
      {
        return;
      }
    lastRefreshMs = nowMs;

    QStringList calls;
    QHash<QString, KnownCqSeed> cqSeeds;
    QHash<QString, KnownCqCallSeed> cqCallSeeds;
    QHash<QString, A7MessageSeed> a7Seeds;
    bool const seedKnownCqReplay = external_known_cq_seed_enabled ();
    bool const seedExternalA7 = external_a7_seed_enabled ();
    for (QString const& path : local_hash_seed_paths ())
      {
        collect_hash_seed_calls_from_tail (path, calls, kHashSeedRefreshTailBytes);
        if (seedKnownCqReplay && external_known_cq_source_allowed (path))
          {
            collect_known_cq_call_seeds_from_tail (path, cqCallSeeds,
                                                   kHashSeedRefreshTailBytes,
                                                   currentNutc);
            collect_known_cq_seeds_from_tail (path, cqSeeds, kHashSeedRefreshTailBytes,
                                              currentNutc);
          }
        if (seedExternalA7)
          {
            collect_a7_message_seeds_from_tail (path, a7Seeds, kHashSeedRefreshTailBytes,
                                                currentNutc);
          }
      }

    for (QString const& call : calls)
      {
        QByteArray const field = call.toLatin1 ().leftJustified (13, ' ', true);
        ftx_ft8_stage4_seed_hash_call_c (field.constData ());
      }
    if (seedKnownCqReplay)
      {
        for (KnownCqSeed const& seed : cqSeeds)
          {
            if (!stable_recent_known_cq_seed (seed, currentNutc))
              {
                continue;
              }
            QByteArray const callField =
                seed.call.toLatin1 ().leftJustified (12, ' ', true);
            QByteArray const gridField =
                seed.grid.toLatin1 ().leftJustified (4, ' ', true);
            int const repeatCount = qBound (1, seed.hits, 4);
            for (int repeat = 0; repeat < repeatCount; ++repeat)
              {
                ftx_ft8_stage4_seed_known_cq_c (callField.constData (),
                                                gridField.constData (), seed.freq,
                                                seed.dt, seed.nutc);
              }
          }
        for (KnownCqCallSeed const& seed : cqCallSeeds)
          {
            if (!stable_recent_known_cq_call_seed (seed, currentNutc))
              {
                continue;
              }
            QByteArray const callField =
                seed.call.toLatin1 ().leftJustified (12, ' ', true);
            int const repeatCount = qBound (1, seed.hits, 4);
            for (int repeat = 0; repeat < repeatCount; ++repeat)
              {
                ftx_ft8_stage4_seed_known_cq_call_c (callField.constData (),
                                                     seed.freq, seed.dt, seed.nutc);
              }
          }
      }
    if (!seedExternalA7)
      {
        return;
      }
    QList<A7MessageSeed> a7SeedList = a7Seeds.values ();
	    std::sort (a7SeedList.begin (), a7SeedList.end (),
	               [currentNutc] (A7MessageSeed const& lhs, A7MessageSeed const& rhs) {
                 if (lhs.priority != rhs.priority)
                   {
                     return lhs.priority > rhs.priority;
                   }
	                 int const lhsAge = currentNutc > 0
	                     ? ft8_nutc_delta_seconds (currentNutc, lhs.nutc)
	                     : 0;
                 int const rhsAge = currentNutc > 0
                     ? ft8_nutc_delta_seconds (currentNutc, rhs.nutc)
                     : 0;
                 if (lhsAge != rhsAge)
                   {
                     return lhsAge < rhsAge;
                   }
                 return lhs.hits > rhs.hits;
               });
    int seededA7 = 0;
    int insertedA7 = 0;
    int seededA7Pairs = 0;
	    for (A7MessageSeed const& seed : a7SeedList)
	      {
        if (seededA7 >= kExternalA7SeedLimit || insertedA7 >= kExternalA7InsertLimit)
	          {
	            break;
	          }
        bool const pairSeed = a7_seed_is_directed_pair (seed);
        if (pairSeed && seededA7Pairs >= kExternalA7PairSeedLimit)
          {
            continue;
          }
        int const jseq = std::abs ((seed.nutc / 5) % 2);
        QByteArray const messageField =
            seed.message.toLatin1 ().leftJustified (kDecodedChars, ' ', true);
        int const repeatCount = seed.priority >= 3 ? qBound (1, seed.hits, 3) : 1;
        int const allowedRepeats = std::min (repeatCount,
                                             kExternalA7InsertLimit - insertedA7);
        for (int repeat = 0; repeat < allowedRepeats; ++repeat)
          {
            ftx_ft8_a7_save_c (jseq, seed.dt, seed.freq, messageField.constData ());
          }
        insertedA7 += allowedRepeats;
        ++seededA7;
        if (pairSeed)
          {
            ++seededA7Pairs;
          }
      }
  }

  QString build_row (QString const& utcPrefix, int snr, float dt, float freq,
                     int nap, float qual, char const* decodeds, int index)
  {
    QString decoded = decode_fallback (decodeds, index);
    if (decoded.size () < kDecodedChars)
      {
        decoded = decoded.leftJustified (kDecodedChars, QLatin1Char {' '});
      }
    decoded = decoded.left (kDecodedChars);
    if (nap != 0 && qual < 0.17f && decoded.size () >= kDecodedChars)
      {
        decoded[kDecodedChars - 1] = QLatin1Char {'?'};
      }
    QByteArray const decodedLatin = decoded.toLatin1 ();
    QByteArray annot = "  ";
    if (nap != 0)
      {
        annot = QStringLiteral ("a%1").arg (nap).leftJustified (2, QLatin1Char {' '}).toLatin1 ();
      }
    QString row = QStringLiteral ("%1%2%3 ~  %4 %5")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (QString::fromLatin1 (decodedLatin.constData (), decodedLatin.size ()))
        .arg (QString::fromLatin1 (annot.constData (), 2));
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, int nout,
                          int const* snrs, float const* dts, float const* freqs,
                          int const* naps, float const* quals, char const* decodeds)
  {
    QStringList rows;
    rows.reserve (qBound (0, nout, kFt8MaxLines));
    for (int i = 0; i < qBound (0, nout, kFt8MaxLines); ++i)
      {
        rows << build_row (utcPrefix, snrs[i], dts[i], freqs[i], naps[i], quals[i],
                           decodeds, i);
      }
    return rows;
  }

  void log_ft8_dsp_rollout_once ()
  {
    static std::once_flag once;
    std::call_once (once, [] {
      LOG_INFO ("FT8 DSP rollout active: stage=" << kFt8StableDspStage
                << " downsample=cpp subtract=cpp saved_subtractions=cpp async_core=cpp"
                << " (runtime baseline)");
    });
  }
}

namespace decodium
{
namespace ft8
{

FT8DecodeWorker::FT8DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT8DecodeWorker::cancelCurrentDecode ()
{
  set_ft8_stage4_cancel (true);
}

void FT8DecodeWorker::beginShutdown ()
{
  m_shuttingDown.store (true, std::memory_order_relaxed);
  set_ft8_stage4_cancel (true);
}

void FT8DecodeWorker::decode (DecodeRequest const& request)
{
  QElapsedTimer totalTimer;
  totalTimer.start ();
  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  apply_decode_thread_limit (request.threadCount);
  int const activeThreads = active_decode_thread_limit ();
  set_ft8_stage4_cancel (false);
  log_ft8_dsp_rollout_once ();
  QElapsedTimer waitTimer;
  waitTimer.start ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  qint64 const waitMs = waitTimer.elapsed ();

  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  seed_ft8_pack77_hashes_from_local_logs_once ();
  refresh_ft8_hash_context_from_recent_logs (request.nutc);
  ftx_ft8_stage4_apply_hash_seed_cache_c ();
  ftx_ft8_stage4_set_deadline_ms_c (ft8_stage4_deadline_ms_from_now (request.maxDecodeMs));
  // The promoted C++ Stage4 path still avoids the full JTDX ft8b subpass
  // engine, but a second candidate cycle with RX-frequency sensitivity 2
  // recovers repeatable JTDX-only decodes while staying under the existing
  // live deadline. Keep early and supplemental passes on the cheaper profile.
  constexpr bool effectiveLowThresholds {false};
  bool const effectiveSubpass {request.subpass};  // F0: era hardcoded false; ora pilotato dal toggle ft8SubpassHarvest
  bool const fullLiveDecode =
      request.ndepth >= 3
      && !request.supplemental
      && request.maxDecodeMs >= 6000;
  int const effectiveCycles = fullLiveDecode ? 2 : 1;
  int const effectiveRxFreqSensitivity = fullLiveDecode ? 2 : 1;
  ftx_ft8_stage4_set_decode_options_c (effectiveLowThresholds ? 1 : 0,
                                       effectiveSubpass ? 1 : 0,
                                       effectiveCycles,
                                       effectiveRxFreqSensitivity,
                                       qBound (1, request.ncandthin, 100));
  // In the full live pass, request the Stage4 focused-window rescue without
  // promoting the whole-band decode to depth 4 or enabling global OSD.
  bool const fullLiveFocusedRescue = fullLiveDecode;
  bool const supplementalRequested =
      request.supplemental || request.ndepth >= 4 || fullLiveFocusedRescue;
  bool const osdSupplementalRequested = request.supplemental || request.ndepth >= 4;
  ftx_ft8_stage4_set_supplemental_c (supplementalRequested ? 1 : 0);
  // Harvest subpass: dig FRESH (reset slot state, a7 preserved) so the
  // parallelized subpass re-scans for weak signals, not the deep residual.
  ftx_ft8_stage4_set_force_fresh_slot_c (effectiveSubpass ? 1 : 0);
  ftx_ft8_stage4_set_freqpart_c (effectiveSubpass ? 8 : 0);
  // Harvest: detection piu' aggressiva (sweep S4) -> +mid-weak. Deep invariato.
  ftx_ft8_stage4_set_syncmin_scale_c (effectiveSubpass ? 0.55f : 1.0f);
  ftx_ft8_stage4_set_decode_syncmin_c (effectiveSubpass ? 2 : -1);
  // Turbo Feedback: estende belief-propagation a 50 iter (default 30).
  ftx_ft8_stage4_set_ldpc_max_iter_c (request.turboFeedbackEnabled ? 50 : 30);
  bool const wantOsd = request.osdFollowup
                       || (osdSupplementalRequested && request.ndepth >= 3)
                       || request.neuralSyncEnabled;
  if (osdSupplementalRequested && request.ndepth >= 4)
    {
      ftx_ft8_stage4_set_ldpc_osd_c (3, 4);
    }
  else if (wantOsd)
    {
      ftx_ft8_stage4_set_ldpc_osd_c (3, 3);
    }
  else
    {
      ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
    }

  short int iwave[kFt8SampleCount] {};
  int const copyCount = std::min (static_cast<int>(request.audio.size ()), static_cast<int>(kFt8SampleCount));
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, iwave);
    }

  int snrs[kFt8MaxLines] {};
  float dts[kFt8MaxLines] {};
  float freqs[kFt8MaxLines] {};
  int naps[kFt8MaxLines] {};
  float quals[kFt8MaxLines] {};
  signed char bits77[kFt8MaxLines * kBitsPerMessage] {};
  char decodeds[kFt8MaxLines * kDecodedChars] {};

  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfqso = qBound (0, request.nfqso, 5000);
  int nftx = qBound (0, request.nftx, 5000);
  int nutc = qMax (0, request.nutc);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int nzhsym = qBound (41, request.nzhsym, 50);
  int ndepth = qBound (1, request.ndepth, 4);
  float emedelay = request.emedelay;
  int ncontest = qBound (0, request.ncontest, 16);
  int nagain = request.nagain ? 1 : 0;
  int lft8apon = request.lft8apon ? 1 : 0;
  int ltry_a8 = (nzhsym == 41 || request.lmultift8 != 0) ? 1 : 0;
  int lapcqonly = request.lapcqonly ? 1 : 0;
  int napwid = qBound (0, request.napwid, 200);
  int ldiskdat = request.ldiskdat ? 1 : 0;
  int nout = 0;
  int const requestedDepth = ndepth;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);
  auto hisgrid = to_fortran_field (request.hisgrid, 6);

  QElapsedTimer decodeTimer;
  decodeTimer.start ();
  ftx_ft8_async_decode_stage4_c (iwave, &nqsoprogress, &nfqso, &nftx, &nutc, &nfa, &nfb,
                                 &nzhsym, &ndepth, &emedelay, &ncontest, &nagain,
                                 &lft8apon, &ltry_a8, &lapcqonly, &napwid, mycall.constData (),
                                 hiscall.constData (), hisgrid.constData (), &ldiskdat,
                                 nullptr,
                                 &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                                 &bits77[0], &decodeds[0], &nout);
  qint64 const decodeMs = decodeTimer.elapsed ();
  ftx_ft8_stage4_set_deadline_ms_c (0);
  ftx_ft8_stage4_set_ldpc_osd_c (-1, 0);
  ftx_ft8_stage4_set_decode_options_c (0, 0, 1, 1, 100);
  ftx_ft8_stage4_set_supplemental_c (0);
  ftx_ft8_stage4_set_ldpc_max_iter_c (30);

  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  QString const utcPrefix = format_decode_utc (request.nutc);
  QStringList rows = build_rows (utcPrefix, nout, snrs, dts, freqs, naps, quals, decodeds);
  if (request.neuralSyncEnabled)
    {
      // Stage4 OSD non espone counter; proxy: rapporto decodi vs cap di 5.
      double const score = wantOsd && !rows.isEmpty ()
          ? std::min (1.0, rows.size () / 5.0)
          : 0.0;
      Q_EMIT neuralSyncHit (score);
    }
  if (request.turboFeedbackEnabled)
    {
      // Counter LDPC iter non esposto via callback; proxy: 50 (cap turbo) se decodi presenti, 0 altrimenti.
      // Triggera LED Turbo Feedback (soglia >8 in DecodiumBridge::notifyTurboIterations).
      Q_EMIT turboIterations (rows.isEmpty () ? 0 : 50);
    }
  qint64 const totalMs = totalTimer.elapsed ();
  static std::atomic<qint64> lastMetricLogMs {0};
  if (decodium::logging::should_log_decode_metric (waitMs, decodeMs, totalMs, lastMetricLogMs))
    {
      qInfo().noquote()
          << QStringLiteral ("[DECODEMETRIC] mode=FT8 serial=%1 wait_ms=%2 decode_ms=%3 total_ms=%4 threads_req=%5 threads_active=%6 audio=%7 nout=%8 depth=%9 nfa=%10 nfb=%11 ap=%12 low=%13 subpass=%14 cycles=%15 requested_low=%16 requested_subpass=%17 requested_cycles=%18 requested_depth=%19 supplemental=%20 thread=0x%21 freqpart=%22")
                 .arg (request.serial)
                 .arg (waitMs)
                 .arg (decodeMs)
                 .arg (totalMs)
                 .arg (request.threadCount)
                 .arg (activeThreads)
                 .arg (request.audio.size ())
                 .arg (nout)
                 .arg (ndepth)
                 .arg (nfa)
                 .arg (nfb)
                 .arg (lft8apon)
                 .arg (effectiveLowThresholds ? 1 : 0)
                 .arg (effectiveSubpass ? 1 : 0)
                 .arg (effectiveCycles)
                 .arg (request.lowThresholds ? 1 : 0)
                 .arg (request.subpass ? 1 : 0)
                 .arg (request.nft8Cycles)
                 .arg (requestedDepth)
                 .arg (supplementalRequested ? 1 : 0)
                 .arg (current_thread_id_hex ())
                 .arg (ftx_ft8_freqpart_bins_used_c ());
    }
  Q_EMIT decodeReady (request.serial, rows);
}

void FT8DecodeWorker::resetDecoderState ()
{
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  ftx_ft8_stage4_reset_c ();
}

}
}
