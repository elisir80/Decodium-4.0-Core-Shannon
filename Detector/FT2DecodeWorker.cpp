// -*- Mode: C++ -*-
#include "Detector/FT2DecodeWorker.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>

#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>

#include "Logger.hpp"
#include "commons.h"
#include "Detector/DecodeMetricLogging.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#ifdef _OPENMP
#include <omp.h>
#endif
extern "C"
{
  int ftx_ft2_cpp_dsp_rollout_stage_c ();
  void ftx_ft2_async_decode_stage7_c (short const* iwave, int* nqsoprogress, int* nfqso,
                                      int* nfa, int* nfb, int* ndepth, int* ncontest,
                                      char const* mycall, char const* hiscall,
                                      int* snrs, float* dts, float* freqs, int* naps,
                                      float* quals, signed char* bits77, char* decodeds,
                                      int* nout);
  void ftx_ft2_stage7_set_cancel_c (int cancel);
  void ftx_ft2_set_ap_hash_cache_c (quint32 const* hashes, int count);  // 1.0.294 AP cache Fase 1
}

namespace
{
  constexpr int kFt2AsyncSampleCount {45000};
  constexpr int kFt2MaxLines {100};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};
  constexpr int kFt2StableDspStage {7};
  char constexpr kFt2DspStageEnv[] {"DECODIUM_FT2_CPP_DSP_STAGE"};
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

  QString build_row (QString const& utcPrefix, char marker, int snr, float dt, float freq,
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
    QString row = QStringLiteral ("%1%2%3 %4  %5 %6")
        .arg (snr, 4)
        .arg (dt, 5, 'f', 1)
        .arg (qRound (freq), 5)
        .arg (QChar::fromLatin1 (marker))
        .arg (QString::fromLatin1 (decodedLatin.constData (), decodedLatin.size ()))
        .arg (QString::fromLatin1 (annot.constData (), 2));
    if (!utcPrefix.isEmpty ())
      {
        row.prepend (utcPrefix);
      }
    return row;
  }

  QStringList build_rows (QString const& utcPrefix, char marker, int nout,
                          int const* snrs, float const* dts, float const* freqs,
                          int const* naps, float const* quals, char const* decodeds)
  {
    QStringList rows;
    rows.reserve (qBound (0, nout, kFt2MaxLines));
    for (int i = 0; i < qBound (0, nout, kFt2MaxLines); ++i)
      {
        rows << build_row (utcPrefix, marker, snrs[i], dts[i], freqs[i], naps[i], quals[i],
                           decodeds, i);
      }
    return rows;
  }

  int ft2_dsp_rollout_stage ()
  {
    QByteArray const raw = qgetenv (kFt2DspStageEnv);
    if (raw.isEmpty ())
      {
        return kFt2StableDspStage;
      }

    bool ok = false;
    int const parsed = raw.toInt (&ok);
    if (!ok)
      {
        return kFt2StableDspStage;
      }
    return std::max (0, std::min (7, parsed));
  }

  void set_ft2_stage7_cancel (bool cancel)
  {
    ftx_ft2_stage7_set_cancel_c (cancel ? 1 : 0);
  }

  void log_ft2_dsp_rollout_once ()
  {
    static std::once_flag once;
    std::call_once (once, [] {
      QByteArray const raw = qgetenv (kFt2DspStageEnv);
      int const stage = ft2_dsp_rollout_stage ();
      LOG_INFO ("FT2 DSP rollout active: stage=" << stage
                << " getcandidates=" << (stage >= 1 ? "cpp" : "fortran")
                << " sync2d=" << (stage >= 2 ? "cpp" : "fortran")
                << " bitmetrics=" << (stage >= 3 ? "cpp" : "fortran")
                << " downsample=" << (stage >= 4 ? "cpp" : "fortran")
                << " subtract=" << (stage >= 5 ? "cpp" : "fortran")
                << " ldpc=" << (stage >= 6 ? "cpp" : "fortran")
                << " orchestration=" << (stage >= 7 ? "cpp" : "fortran")
                << (raw.isEmpty ()
                    ? " (default)"
                    : std::string {" via "} + kFt2DspStageEnv + "=" + raw.constData ()));
    });
  }
}

namespace decodium
{
namespace ft2
{

FT2DecodeWorker::FT2DecodeWorker (QObject * parent)
  : QObject {parent}
{
}

void FT2DecodeWorker::markLatestDecodeSerial (quint64 serial)
{
  m_latestDecodeSerial.store (serial, std::memory_order_relaxed);
}

void FT2DecodeWorker::cancelCurrentDecode ()
{
  set_ft2_stage7_cancel (true);
}

void FT2DecodeWorker::beginShutdown ()
{
  m_shuttingDown.store (true, std::memory_order_relaxed);
  set_ft2_stage7_cancel (true);
}

void FT2DecodeWorker::decodeAsync (AsyncDecodeRequest const& request)
{
  QElapsedTimer totalTimer;
  totalTimer.start ();
  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  apply_decode_thread_limit (request.threadCount);
  int const activeThreads = active_decode_thread_limit ();
  set_ft2_stage7_cancel (false);
  log_ft2_dsp_rollout_once ();
  QElapsedTimer waitTimer;
  waitTimer.start ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  qint64 const waitMs = waitTimer.elapsed ();

  short int iwave[kFt2AsyncSampleCount] {};
  int const copyCount = std::min (static_cast<int>(request.audio.size ()), static_cast<int>(kFt2AsyncSampleCount));
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, iwave);
    }

  int snrs[kFt2MaxLines] {};
  float dts[kFt2MaxLines] {};
  float freqs[kFt2MaxLines] {};
  int naps[kFt2MaxLines] {};
  float quals[kFt2MaxLines] {};
  signed char bits77[kFt2MaxLines * kBitsPerMessage] {};
  char decodeds[kFt2MaxLines * kDecodedChars] {};

  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfqso = qBound (0, request.nfqso, 5000);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int ndepth = qMax (1, request.ndepth);
  int ncontest = qBound (0, request.ncontest, 16);
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);

  // 1.0.294 — AP cache Fase 1: passa lo snapshot hash28 (thread_local) prima del decode,
  // azzera subito dopo (così il decode() sincrono non eredita una cache stale).
  ftx_ft2_set_ap_hash_cache_c (request.apHashCache.constData (),
                               static_cast<int> (request.apHashCache.size ()));
  QElapsedTimer decodeTimer;
  decodeTimer.start ();
  ftx_ft2_async_decode_stage7_c (iwave, &nqsoprogress, &nfqso, &nfa, &nfb,
                                 &ndepth, &ncontest, mycall.data (), hiscall.data (),
                                 &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                                 &bits77[0], &decodeds[0], &nout);
  qint64 const decodeMs = decodeTimer.elapsed ();
  ftx_ft2_set_ap_hash_cache_c (nullptr, 0);

  if (m_shuttingDown.load (std::memory_order_relaxed))
    {
      return;
    }
  QString const utcPrefix = format_decode_utc (request.nutc);
  qint64 const totalMs = totalTimer.elapsed ();
  static std::atomic<qint64> lastMetricLogMs {0};
  if (decodium::logging::should_log_decode_metric (waitMs, decodeMs, totalMs, lastMetricLogMs))
    {
      qInfo().noquote()
          << QStringLiteral ("[DECODEMETRIC] mode=FT2-async wait_ms=%1 decode_ms=%2 total_ms=%3 threads_req=%4 threads_active=%5 audio=%6 nout=%7 depth=%8 nfa=%9 nfb=%10 ap_cache=%11 thread=0x%12")
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
                 .arg (request.apHashCache.size ())
                 .arg (current_thread_id_hex ());
    }
  Q_EMIT asyncDecodeReady (build_rows (utcPrefix, '~', nout, snrs, dts, freqs, naps, quals,
                                       decodeds));
}

void FT2DecodeWorker::decode (DecodeRequest const& request)
{
  QElapsedTimer totalTimer;
  totalTimer.start ();
  if (m_shuttingDown.load (std::memory_order_relaxed)
      || request.serial != m_latestDecodeSerial.load (std::memory_order_relaxed))
    {
      return;
    }
  apply_decode_thread_limit (request.threadCount);
  int const activeThreads = active_decode_thread_limit ();
  set_ft2_stage7_cancel (false);
  log_ft2_dsp_rollout_once ();
  QElapsedTimer waitTimer;
  waitTimer.start ();
  QMutexLocker runtime_lock {&decodium::fortran::runtime_mutex ()};
  qint64 const waitMs = waitTimer.elapsed ();

  if (m_shuttingDown.load (std::memory_order_relaxed)
      || request.serial != m_latestDecodeSerial.load (std::memory_order_relaxed))
    {
      return;
    }

  short int iwave[kFt2AsyncSampleCount] {};
  int const copyCount = std::min (static_cast<int>(request.audio.size ()), static_cast<int>(kFt2AsyncSampleCount));
  if (copyCount > 0)
    {
      std::copy_n (request.audio.constBegin (), copyCount, iwave);
    }

  int snrs[kFt2MaxLines] {};
  float dts[kFt2MaxLines] {};
  float freqs[kFt2MaxLines] {};
  int naps[kFt2MaxLines] {};
  float quals[kFt2MaxLines] {};
  signed char bits77[kFt2MaxLines * kBitsPerMessage] {};
  char decodeds[kFt2MaxLines * kDecodedChars] {};

  int nqsoprogress = qBound (0, request.nqsoprogress, 6);
  int nfqso = qBound (0, request.nfqso, 5000);
  int nfa = qBound (0, request.nfa, 5000);
  int nfb = qMax (nfa + 50, qBound (0, request.nfb, 5000));
  int ndepth = qMax (1, request.ndepth);
  int ncontest = qBound (0, request.ncontest, 16);
  int nout = 0;

  auto mycall = to_fortran_field (request.mycall, 12);
  auto hiscall = to_fortran_field (request.hiscall, 12);

  QElapsedTimer decodeTimer;
  decodeTimer.start ();
  ftx_ft2_async_decode_stage7_c (iwave, &nqsoprogress, &nfqso, &nfa, &nfb,
                                 &ndepth, &ncontest, mycall.data (), hiscall.data (),
                                 &snrs[0], &dts[0], &freqs[0], &naps[0], &quals[0],
                                 &bits77[0], &decodeds[0], &nout);
  qint64 const decodeMs = decodeTimer.elapsed ();

  if (m_shuttingDown.load (std::memory_order_relaxed)
      || request.serial != m_latestDecodeSerial.load (std::memory_order_relaxed))
    {
      return;
    }
  LOG_DEBUG ("FT2 decode completed: stage=" << ft2_dsp_rollout_stage ()
             << " nout=" << nout);
  QString const utcPrefix = format_decode_utc (request.nutc);
  qint64 const totalMs = totalTimer.elapsed ();
  static std::atomic<qint64> lastMetricLogMs {0};
  if (decodium::logging::should_log_decode_metric (waitMs, decodeMs, totalMs, lastMetricLogMs))
    {
      qInfo().noquote()
          << QStringLiteral ("[DECODEMETRIC] mode=FT2 serial=%1 wait_ms=%2 decode_ms=%3 total_ms=%4 threads_req=%5 threads_active=%6 audio=%7 nout=%8 depth=%9 nfa=%10 nfb=%11 thread=0x%12")
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
                 .arg (current_thread_id_hex ());
    }
  Q_EMIT decodeReady (request.serial, build_rows (utcPrefix, '~', nout, snrs, dts, freqs, naps,
                                                  quals, decodeds));
}

}
}
