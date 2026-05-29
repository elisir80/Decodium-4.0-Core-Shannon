#include "soundout.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <qmath.h>
#include <QDebug>

#include "Audio/AudioDevice.hpp"

#include "moc_soundout.cpp"

namespace
{
constexpr qreal kSoundOutputHeadroomGain = 0.9;

QString formatSummary(QAudioFormat const& format)
{
  if (!format.isValid()) {
    return QStringLiteral("invalid-format");
  }
  return QStringLiteral("rate=%1 ch=%2 sample=%3 bytes=%4 cfg=%5")
      .arg(format.sampleRate())
      .arg(format.channelCount())
      .arg(static_cast<int>(format.sampleFormat()))
      .arg(format.bytesPerFrame())
      .arg(static_cast<quint32>(format.channelConfig()));
}

QString audioStateName(QAudio::State state)
{
  switch (state) {
  case QAudio::ActiveState: return QStringLiteral("ActiveState");
  case QAudio::SuspendedState: return QStringLiteral("SuspendedState");
  case QAudio::StoppedState: return QStringLiteral("StoppedState");
  case QAudio::IdleState: return QStringLiteral("IdleState");
  }
  return QStringLiteral("UnknownState(%1)").arg(static_cast<int>(state));
}

QString audioErrorName(QAudio::Error error)
{
  switch (error) {
  case QAudio::NoError: return QStringLiteral("NoError");
  case QAudio::OpenError: return QStringLiteral("OpenError");
  case QAudio::IOError: return QStringLiteral("IOError");
  case QAudio::UnderrunError: return QStringLiteral("UnderrunError");
  case QAudio::FatalError: return QStringLiteral("FatalError");
  }
  return QStringLiteral("UnknownError(%1)").arg(static_cast<int>(error));
}

QAudioFormat txPcmFormat(unsigned channels)
{
  QAudioFormat format;
  format.setChannelCount(static_cast<int>(channels));
  format.setChannelConfig(channels == 1
                              ? QAudioFormat::ChannelConfigMono
                              : QAudioFormat::ChannelConfigStereo);
  format.setSampleRate(48000);
  format.setSampleFormat(QAudioFormat::Int16);
  return format;
}

// 1.0.216 — park-and-delete anche su Windows per eliminare lo stutter
// "WASAPI destroy + recreate" ad ogni fine TX FT2 (slot 7.5s). Pre-1.0.216
// retireStream() su Windows faceva reset()+stop()+delete sincrono, e
// il prossimo startTx() ricreava un sink nuovo entro ~50ms: Qt WASAPI
// engine cleanup + new session connect = 100-200ms di stutter audio.
// Ora il sink viene "parcheggiato" (setVolume(0) + disconnect) e
// distrutto dopo una window > slot ma << 2 slot, in modo che la cleanup
// del backend non avvenga MAI dentro la pipeline TX successiva.
bool deferCoreAudioSinkDelete()
{
#if defined(Q_OS_MAC)
  return QOperatingSystemVersion::current()
      >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15);
#elif defined(Q_OS_WIN)
  return true;
#else
  return false;
#endif
}

int parkedSinkLifetimeMs()
{
#if defined(Q_OS_MAC)
  // Mac: keep the retired sink alive beyond the next FT2 slot and release it
  // only when no replacement sink is active. Stopping a retired CoreAudio sink
  // while the next payload is already playing can mute that next payload.
  return 20000;
#elif defined(Q_OS_WIN)
  // Win: 8s = oltre 1 slot FT2 (7.5s), sotto 2 slot. Max ~2 sink parked
  // contemporanei, no accumulo memoria.
  return 8000;
#else
  return 30000;
#endif
}

}

void SoundOutput::deleteRetiredStreamAfterCoreAudioCallbacks(QAudioSink *stream,
                                                             QString const& reason,
                                                             int delayMs)
{
  if (!stream) {
    return;
  }

  stream->disconnect();
  stream->setParent(nullptr);
#if defined(Q_OS_MAC)
  stream->setVolume(0.0f);
  qInfo() << "TX SoundOutput CoreAudio sink left parked without delayed Qt/CoreAudio callbacks:"
          << reason;
  return;
#endif

  QObject *context = QCoreApplication::instance();
  if (!context) {
    delete stream;
    return;
  }

  QPointer<SoundOutput> owner(this);
  QPointer<QAudioSink> guard(stream);
  int const lifetimeMs = delayMs > 0 ? delayMs : parkedSinkLifetimeMs();
  QTimer::singleShot(lifetimeMs, context, [owner, guard, reason]() {
    if (!guard) {
      return;
    }
    guard->disconnect();
    if (guard->state() == QAudio::StoppedState) {
#if defined(Q_OS_MAC)
      qInfo() << "TX SoundOutput CoreAudio sink left parked stopped after lifetime:" << reason;
      return;
#else
      guard->deleteLater();
      qInfo() << "TX SoundOutput sink deferred delete released:" << reason;
      return;
#endif
    }
#if defined(Q_OS_MAC)
    if (owner && owner->m_stream && owner->m_stream.data() != guard.data()) {
      qInfo() << "TX SoundOutput CoreAudio sink left parked during active stream:"
              << reason
              << "retired_state=" << audioStateName(guard->state())
              << "active_state=" << audioStateName(owner->m_stream->state());
      return;
    }
#endif

    // Il sink potrebbe non essere arrivato in StoppedState (es. underrun
    // cronico o CoreAudio rimasto Idle). Windows puo' forzare stop() dopo la
    // safe-window; macOS/Qt 6.11 puo' invece invalidare QSocketNotifier se
    // fermiamo o distruggiamo il sink differito.
#if defined(Q_OS_WIN)
    QString const parkedState = audioStateName(guard->state());
    guard->stop();
    guard->deleteLater();
    qInfo() << "TX SoundOutput sink parked stop+delete after lifetime:"
            << reason
            << "state=" << parkedState;
    return;
#elif defined(Q_OS_MAC)
    QString const parkedState = audioStateName(guard->state());
    QString const parkedError = audioErrorName(guard->error());
    qInfo() << "TX SoundOutput CoreAudio sink left parked after lifetime:"
            << reason
            << "state=" << parkedState
            << "error=" << parkedError;
    return;
#else
    qInfo() << "TX SoundOutput sink parked to avoid backend crash:"
            << reason
            << "state=" << audioStateName(guard->state())
            << "error=" << audioErrorName(guard->error());
#endif
  });
}

SoundOutput::~SoundOutput()
{
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_streamDevice.clear();
  m_sourceDevice.clear();
  m_coreAudioKeepAlive = false;
  retireStream(QStringLiteral("dtor"));
}

bool SoundOutput::checkStream() const
{
  bool result {false};
  Q_ASSERT_X(m_stream, "SoundOutput", "programming error");
  if (m_stream) {
    QString const context = QStringLiteral("device=\"%1\", format=%2, state=%3, qt-error=%4, buffer=%5, bytes-free=%6")
        .arg(m_device.description().isEmpty() ? QStringLiteral("<default output>") : m_device.description(),
             formatSummary(m_stream->format()),
             audioStateName(m_stream->state()),
             audioErrorName(m_stream->error()))
        .arg(m_stream->bufferSize())
        .arg(m_stream->bytesFree());
    switch (m_stream->error()) {
    case QAudio::OpenError:
      Q_EMIT error(tr("Audio TX output open error: Qt could not open the selected output device. %1").arg(context));
      break;
    case QAudio::IOError:
      Q_EMIT error(tr("Audio TX output write error: Qt reported an I/O failure while writing samples. %1").arg(context));
      break;
    case QAudio::UnderrunError:
      // Keep underruns non-fatal and report status.
      Q_EMIT status(tr("Audio TX output underrun: the audio sink fell behind but will continue. %1").arg(context));
      result = true;
      break;
    case QAudio::FatalError:
      Q_EMIT error(tr("Audio TX output fatal error: the selected output device is not usable now. %1").arg(context));
      break;
    case QAudio::NoError:
      result = true;
      break;
    }
  }
  return result;
}

void SoundOutput::setFormat(QAudioDevice const& device, unsigned channels, int frames_buffered)
{
  Q_ASSERT(0 < channels && channels < 3);
  m_device = device;
  m_channels = channels;
  m_framesBuffered = frames_buffered;
}

void SoundOutput::restart(QIODevice* source)
{
  QElapsedTimer totalTimer;
  totalTimer.start();
  qint64 retireMs = 0;
  qint64 createMs = 0;
  qint64 configMs = 0;
  qint64 startMs = 0;
  qint64 pumpMs = 0;

  QAudioFormat const format = txPcmFormat(m_channels);

  m_pumpTimer.stop();
  m_sourceDevice.clear();
  m_pendingWrite.clear();

#if defined(Q_OS_MAC)
  if (m_stream
      && !m_streamDevice.isNull()
      && m_openDeviceId == m_device.id()
      && m_stream->format().sampleRate() == format.sampleRate()
      && m_stream->format().channelCount() == format.channelCount()
      && m_stream->format().sampleFormat() == format.sampleFormat()
      && m_stream->state() != QAudio::StoppedState) {
    QElapsedTimer phaseTimer;
    phaseTimer.start();
    m_coreAudioKeepAlive = false;
    m_sourceDevice = source;
    m_stream->setVolume(static_cast<float>(m_volume));
    error_ = false;
    pumpAudio();
    pumpMs = phaseTimer.elapsed();
    if (m_sourceDevice || !m_pendingWrite.isEmpty()) {
      m_pumpTimer.start();
    }
    qInfo().noquote() << "[TX-TL] sound_output_restart"
                      << "total_ms=" << totalTimer.elapsed()
                      << "retire_ms=0"
                      << "create_ms=0"
                      << "config_ms=0"
                      << "start_ms=0"
                      << "pump_ms=" << pumpMs
                      << "reuse=1"
                      << "state=" << audioStateName(m_stream->state())
                      << "error=" << audioErrorName(m_stream->error())
                      << "pending_bytes=" << m_pendingWrite.size()
                      << "source_pos=" << (source ? source->pos() : -1)
                      << "source_size=" << (source ? source->size() : -1)
                      << "dev=" << m_device.description();
    Q_EMIT status(QString("after_start: state=%1 err=%2 bufSize=%3 reuse=1")
      .arg(m_stream->state()).arg(m_stream->error()).arg(m_stream->bufferSize()));
    return;
  }
  m_coreAudioKeepAlive = false;
#endif

  m_streamDevice.clear();
  QElapsedTimer phaseTimer;
  phaseTimer.start();
  retireStream(QStringLiteral("restart"));
  retireMs = phaseTimer.elapsed();

  if (!m_device.id().isEmpty()) {
    phaseTimer.restart();
    // Keep TX format fixed to 48 kHz / Int16 to match the modulator output.
    // On macOS many devices report Float32 as their native format; Qt/CoreAudio
    // can still convert this stream internally, so we log unsupported-native
    // formats instead of aborting early.
    QAudioFormat const preferred = m_device.preferredFormat();
    Q_EMIT status(QStringLiteral("TX audio fmt req=%1 pref=%2 dev=%3")
                      .arg(formatSummary(format))
                      .arg(formatSummary(preferred))
                      .arg(m_device.description()));

    if (!format.isValid()) {
      Q_EMIT error(tr("Audio TX format invalid: device=\"%1\", requested=%2, preferred=%3")
                   .arg(m_device.description(),
                        formatSummary(format),
                        formatSummary(preferred)));
      return;
    }
    if (!m_device.isFormatSupported(format)) {
      Q_EMIT status(tr("TX audio: device does not natively support %1 Hz / Int16 "
                       "(preferred: %2 Hz / %3 ch) – relying on Qt/CoreAudio conversion")
                    .arg(format.sampleRate())
                    .arg(preferred.sampleRate())
                    .arg(preferred.channelCount()));
    }

    QElapsedTimer createTimer;
    createTimer.start();
    m_stream.reset(new QAudioSink(m_device, format));
    createMs = createTimer.elapsed();
    m_openDeviceId = m_device.id();
    checkStream();
    m_stream->setVolume(static_cast<float>(m_volume));
    error_ = false;
    Q_EMIT status(QStringLiteral("TX audio sink opened fmt=%1 dev=%2")
                      .arg(formatSummary(m_stream->format()))
                      .arg(m_device.description()));
    qInfo().noquote() << "TX SoundOutput sink opened"
                      << "fmt=" << formatSummary(m_stream->format())
                      << "dev=" << m_device.description();

    connect(m_stream.data(), &QAudioSink::stateChanged,
            this, &SoundOutput::handleStateChanged);
  }

  if (!m_stream) {
    if (!error_) {
      error_ = true;
      Q_EMIT error(tr("Audio TX output device is not configured: select an output device in Settings > Audio."));
    }
    return;
  } else {
    error_ = false;
  }

#if defined(Q_OS_MAC)
  constexpr int kDefaultTxBufferFrames = 16384;
#else
  constexpr int kDefaultTxBufferFrames = 49152;
#endif

  // Buffer sizing: Windows + USB Audio CODEC needs a generous buffer to
  // prevent underruns mid-TX. macOS keeps the Decodium3-sized default
  // buffer so FT2 TX attack latency stays close to the legacy behavior.
  phaseTimer.restart();
  if (m_framesBuffered > 0) {
    m_stream->setBufferSize(
        static_cast<qsizetype>(m_stream->format().bytesForFrames(m_framesBuffered)));
  } else {
    m_stream->setBufferSize(
        static_cast<qsizetype>(m_stream->format().bytesForFrames(kDefaultTxBufferFrames)));
  }
  configMs = phaseTimer.elapsed();

#if defined(Q_OS_MAC)
  m_sourceDevice = source;
  phaseTimer.restart();
  m_streamDevice = m_stream->start();
  startMs = phaseTimer.elapsed();
  if (!m_streamDevice) {
    Q_EMIT error(tr("Audio TX output start failed: Qt did not return a writable sink device. device=\"%1\", format=%2, state=%3, qt-error=%4")
                 .arg(m_device.description(),
                      formatSummary(m_stream->format()),
                      audioStateName(m_stream->state()),
                      audioErrorName(m_stream->error())));
    return;
  }
  qInfo().noquote() << "TX SoundOutput start"
                    << "state=" << audioStateName(m_stream->state())
                    << "error=" << audioErrorName(m_stream->error())
                    << "bytesFree=" << m_stream->bytesFree()
                    << "buffer=" << m_stream->bufferSize();
  phaseTimer.restart();
  pumpAudio();
  pumpMs = phaseTimer.elapsed();
  if (m_sourceDevice || !m_pendingWrite.isEmpty()) {
    m_pumpTimer.start();
  }
#else
  phaseTimer.restart();
  m_stream->start(source);
  startMs = phaseTimer.elapsed();
#endif
  qInfo().noquote() << "[TX-TL] sound_output_restart"
                    << "total_ms=" << totalTimer.elapsed()
                    << "retire_ms=" << retireMs
                    << "create_ms=" << createMs
                    << "config_ms=" << configMs
                    << "start_ms=" << startMs
                    << "pump_ms=" << pumpMs
                    << "state=" << (m_stream ? audioStateName(m_stream->state()) : QStringLiteral("no-stream"))
                    << "error=" << (m_stream ? audioErrorName(m_stream->error()) : QStringLiteral("no-stream"))
                    << "pending_bytes=" << m_pendingWrite.size()
                    << "source_pos=" << (source ? source->pos() : -1)
                    << "source_size=" << (source ? source->size() : -1)
                    << "dev=" << m_device.description();
  // diagnostico: stato subito dopo start
  Q_EMIT status(QString("after_start: state=%1 err=%2 bufSize=%3")
    .arg(m_stream->state()).arg(m_stream->error()).arg(m_stream->bufferSize()));
}

void SoundOutput::suspend()
{
  if (m_stream && QAudio::ActiveState == m_stream->state()) {
    m_stream->suspend();
    checkStream();
  }
}

void SoundOutput::resume()
{
  if (m_stream && QAudio::SuspendedState == m_stream->state()) {
    m_stream->resume();
    checkStream();
  }
}

void SoundOutput::finishPlayback()
{
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_sourceDevice.clear();
#if defined(Q_OS_MAC)
  if (m_stream && !m_streamDevice.isNull()) {
    m_coreAudioKeepAlive = true;
    m_stream->setVolume(0.0f);
    pumpAudio();
    m_pumpTimer.start();
    Q_EMIT status(QStringLiteral("TX SoundOutput CoreAudio sink kept warm after finish"));
    qInfo().noquote() << "[TX-TL] sound_output_retire"
                      << "reason=finish"
                      << "mode=keepalive"
                      << "elapsed_ms=0";
    return;
  }
#endif
  m_streamDevice.clear();
  retireStream(QStringLiteral("finish"));
}

void SoundOutput::reset()
{
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_streamDevice.clear();
  m_sourceDevice.clear();
  m_coreAudioKeepAlive = false;
  if (m_stream) {
    if (deferCoreAudioSinkDelete()) {
      retireStream(QStringLiteral("reset"));
      return;
    }
    m_stream->reset();
    checkStream();
  }
}

void SoundOutput::stop()
{
  m_pumpTimer.stop();
  m_pendingWrite.clear();
  m_sourceDevice.clear();
#if defined(Q_OS_MAC)
  if (m_stream && !m_streamDevice.isNull()) {
    m_coreAudioKeepAlive = true;
    m_stream->setVolume(0.0f);
    pumpAudio();
    m_pumpTimer.start();
    Q_EMIT status(QStringLiteral("TX SoundOutput CoreAudio sink kept warm after stop"));
    qInfo().noquote() << "[TX-TL] sound_output_retire"
                      << "reason=stop"
                      << "mode=keepalive"
                      << "elapsed_ms=0";
    return;
  }
#endif
  m_streamDevice.clear();
  m_coreAudioKeepAlive = false;
  retireStream(QStringLiteral("stop"));
}

void SoundOutput::retireStream(QString const& reason)
{
  if (!m_stream) {
    return;
  }

  QElapsedTimer timer;
  timer.start();
  QAudioSink *stream = m_stream.take();
  m_openDeviceId.clear();
  stream->disconnect(this);
  stream->disconnect();

  if (deferCoreAudioSinkDelete()) {
    stream->setVolume(0.0f);
    Q_EMIT status(QStringLiteral("TX SoundOutput CoreAudio sink retired without immediate stop: %1").arg(reason));
    deleteRetiredStreamAfterCoreAudioCallbacks(stream, reason);
#if defined(Q_OS_MAC)
    qInfo().noquote() << "[TX-TL] sound_output_retire"
                      << "reason=" << reason
                      << "mode=parked_mac"
                      << "elapsed_ms=" << timer.elapsed();
#else
    qInfo().noquote() << "[TX-TL] sound_output_retire"
                      << "reason=" << reason
                      << "mode=defer"
                      << "elapsed_ms=" << timer.elapsed()
                      << "state=" << audioStateName(stream->state())
                      << "error=" << audioErrorName(stream->error());
#endif
    return;
  }

  QAudio::State const beforeState = stream->state();
  stream->reset();
  stream->stop();
  qInfo().noquote() << "[TX-TL] sound_output_retire"
                    << "reason=" << reason
                    << "mode=stop_delete"
                    << "elapsed_ms=" << timer.elapsed()
                    << "state_before=" << audioStateName(beforeState)
                    << "state_after=" << audioStateName(stream->state())
                    << "error=" << audioErrorName(stream->error());
  delete stream;
}

qreal SoundOutput::attenuation() const
{
  return -(20. * qLn(m_volume) / qLn(10.));
}

void SoundOutput::setAttenuation(qreal a)
{
  Q_ASSERT(0. <= a && a <= 999.);
  m_volume = kSoundOutputHeadroomGain * qPow(10.0, -a / 20.0);
  if (m_stream)
    m_stream->setVolume(static_cast<float>(m_volume));
}

void SoundOutput::resetAttenuation()
{
  m_volume = kSoundOutputHeadroomGain;
  if (m_stream)
    m_stream->setVolume(static_cast<float>(m_volume));
}

void SoundOutput::pumpAudio()
{
#if defined(Q_OS_MAC)
  if (!m_stream || !m_streamDevice) {
    m_pumpTimer.stop();
    return;
  }

  constexpr qsizetype maxChunkBytes = 16384;
  const int bytesPerFrame = qMax(1, m_stream->format().bytesPerFrame());
  qint64 totalRead = 0;
  qint64 totalWritten = 0;
  int chunks = 0;

  while (true) {
    if (!m_pendingWrite.isEmpty()) {
      const qsizetype allowed = qMin<qsizetype>(m_stream->bytesFree(), m_pendingWrite.size());
      const qsizetype writable = allowed - (allowed % bytesPerFrame);
      if (writable <= 0) {
        break;
      }

      const qint64 written = m_streamDevice->write(m_pendingWrite.constData(), writable);
      if (written < 0) {
        m_pumpTimer.stop();
        Q_EMIT error(tr("Audio TX output write error: Qt rejected buffered audio data. device=\"%1\", format=%2, state=%3, qt-error=%4")
                     .arg(m_device.description(),
                          formatSummary(m_stream->format()),
                          audioStateName(m_stream->state()),
                          audioErrorName(m_stream->error())));
        return;
      }

      if (written > 0) {
        m_pendingWrite.remove(0, static_cast<int>(written));
        totalWritten += written;
        ++chunks;
      }

      if (!m_pendingWrite.isEmpty()) {
        break;
      }
    }

    if (!m_sourceDevice) {
      if (m_pendingWrite.isEmpty()) {
        if (m_coreAudioKeepAlive) {
          qsizetype request = qMin<qsizetype>(m_stream->bytesFree(), 1024);
          request -= request % bytesPerFrame;
          if (request > 0) {
            QByteArray silence(static_cast<int>(request), Qt::Uninitialized);
            silence.fill('\0');
            const qint64 written = m_streamDevice->write(silence.constData(), silence.size());
            if (written > 0) {
              totalWritten += written;
              ++chunks;
            }
          }
          break;
        }
        m_pumpTimer.stop();
      }
      break;
    }

    qsizetype request = qMin<qsizetype>(m_stream->bytesFree(), maxChunkBytes);
    request -= request % bytesPerFrame;
    if (request <= 0) {
      break;
    }

    QByteArray chunk(static_cast<int>(request), Qt::Uninitialized);
    const qint64 read = m_sourceDevice->read(chunk.data(), request);
    if (read < 0) {
      m_pumpTimer.stop();
      Q_EMIT error(tr("Audio TX source read error: Decodium could not read generated TX audio before writing it. device=\"%1\", format=%2")
                   .arg(m_device.description(), formatSummary(m_stream->format())));
      return;
    }

    if (read == 0) {
      m_sourceDevice.clear();
      if (m_pendingWrite.isEmpty()) {
        if (!m_coreAudioKeepAlive) {
          m_pumpTimer.stop();
        }
      }
      break;
    }

    chunk.truncate(static_cast<int>(read));
    totalRead += read;
    const qint64 written = m_streamDevice->write(chunk.constData(), chunk.size());
    if (written < 0) {
      m_pumpTimer.stop();
      Q_EMIT error(tr("Audio TX output write error: Qt rejected generated TX audio data. device=\"%1\", format=%2, state=%3, qt-error=%4")
                   .arg(m_device.description(),
                        formatSummary(m_stream->format()),
                        audioStateName(m_stream->state()),
                        audioErrorName(m_stream->error())));
      return;
    }

    if (written < chunk.size()) {
      m_pendingWrite = chunk.mid(static_cast<int>(written));
      if (written > 0) {
        totalWritten += written;
        ++chunks;
      }
      break;
    }
    if (written > 0) {
      totalWritten += written;
      ++chunks;
    }
  }
  if ((totalRead > 0 || totalWritten > 0)
      && qEnvironmentVariableIsSet("DECODIUM_TX_PUMP_TRACE")) {
    qInfo().noquote() << "TX SoundOutput pump"
                      << "read=" << totalRead
                      << "written=" << totalWritten
                      << "chunks=" << chunks
                      << "pending=" << m_pendingWrite.size()
                      << "state=" << audioStateName(m_stream->state())
                      << "bytesFree=" << m_stream->bytesFree();
  }
#endif
}

void SoundOutput::handleStateChanged(QAudio::State newState)
{
  switch (newState) {
  case QAudio::IdleState:
    pumpAudio();
    Q_EMIT status(tr("Idle"));
    break;
  case QAudio::ActiveState:
    Q_EMIT status(tr("Sending"));
    break;
  case QAudio::SuspendedState:
    Q_EMIT status(tr("Suspended"));
    break;
  case QAudio::StoppedState:
#if defined(Q_OS_MAC)
    if (m_coreAudioKeepAlive) {
      Q_EMIT status(tr("TX output stopped while parked"));
      break;
    }
#endif
    if (!checkStream())
      Q_EMIT status(tr("Audio TX output stopped with error: device=\"%1\", state=%2")
                    .arg(m_device.description().isEmpty() ? QStringLiteral("<default output>") : m_device.description(),
                         audioStateName(newState)));
    else
      Q_EMIT status(tr("Stopped"));
    break;
  default:
    break;
  }
}
