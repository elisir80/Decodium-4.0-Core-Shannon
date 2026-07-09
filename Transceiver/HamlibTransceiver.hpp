#ifndef HAMLIB_TRANSCEIVER_HPP_
#define HAMLIB_TRANSCEIVER_HPP_

#include <QString>
#include <hamlib/rig.h>

#include "TransceiverFactory.hpp"
#include "PollingTransceiver.hpp"
#include "pimpl_h.hpp"

class QTimer;

// hamlib transceiver and PTT mostly delegated directly to hamlib Rig class
class HamlibTransceiver final
  : public PollingTransceiver
{
  Q_OBJECT                      // for translation context

public:
  static void register_transceivers (logger_type *, TransceiverFactory::Transceivers *);
  static void unregister_transceivers ();

  explicit HamlibTransceiver (logger_type *, unsigned model_number, TransceiverFactory::ParameterPack const&,
                              QObject * parent = nullptr);
  explicit HamlibTransceiver (logger_type *, TransceiverFactory::PTTMethod ptt_type, QString const& ptt_port,
                              QObject * parent = nullptr);
  ~HamlibTransceiver ();

  void send_morse (QString const&, int) noexcept override;  // keying CW via Hamlib

private:
  void load_user_settings ();
  int do_start () override;
  void do_stop () override;
  void do_frequency (Frequency, MODE, bool no_ignore) override;
  void do_tx_frequency (Frequency, MODE, bool no_ignore) override;
  void do_mode (MODE) override;
  void do_ptt (bool) override;
  void do_tune (bool) override;

  void do_poll () override;
  void poll_transmit_telemetry (bool force_signal = false);
  void start_cat_keep_alive_timer ();
  void stop_cat_keep_alive_timer ();
  void poll_cat_keep_alive ();
  void schedule_transmit_telemetry_burst ();
  vfo_t frequency_poll_vfo () const;
  bool poll_vfo_frequency (vfo_t, freq_t *, QString const&);
  void note_frequency_poll_success ();
  void note_frequency_poll_failure (int, QString const&);
  bool cat_write_backoff_active () const;
  bool suppress_cat_write_during_backoff (QString const& operation) const;
  int ptt_off_attempt_limit (bool shutdown) const;

  bool ptt_on_ = false;
  bool ptt_off_failed_recently_ = false;
  bool rig_split_control_enabled_ = true;
  bool explicit_frequency_poll_vfo_ = false;
  bool frequency_poll_vfo_logged_ = false;
  bool poll_passive_state_ = true;
  bool poll_frequency_state_ = true;
  bool poll_ptt_state_ = true;
  bool adaptive_frequency_poll_ = false;
  bool cat_keep_alive_ = false;
  int cat_keep_alive_failures_ = 0;
  QTimer * cat_keep_alive_timer_ {nullptr};
  bool do_pwr_ = false;
  bool do_pwr2_ = false;
  bool do_swr_ = false;
  bool do_alc_ = false;  // 1.0.323 — lettura RIG_LEVEL_ALC in TX (ALC automatico, fase 1 display)
  bool alc_probe_pending_ = false;

  // 1.0.204 — throttle telemetry polling: SWR/PWR add ~300ms per poll on slow
  // rigs (FT-991 38400 baud). Polling at full 1Hz blocks the worker thread
  // for ~470ms which propagates as main-thread stall when sendStateSync runs
  // concurrently. Skip telemetry on N-1 ticks of every N (default 4) when
  // any telemetry channel is enabled.
  static constexpr int kTelemetrySkipRatio_ = 4;
  static constexpr int kCatKeepAliveIntervalMs_ = 300;
  static constexpr int kCatKeepAliveMaxFailures_ = 3;
  static constexpr int kFrequencyPollMaxFailures_ = 2;
  static constexpr int kFrequencyPollInitialBackoffTicks_ = 2;
  static constexpr int kFrequencyPollMaxBackoffTicks_ = 10;
  int telemetry_tick_ = 0;
  int frequency_poll_failures_ = 0;
  int frequency_poll_skip_ticks_ = 0;
  int frequency_poll_backoff_ticks_ = kFrequencyPollInitialBackoffTicks_;

  class impl;
  pimpl<impl> m_;
};

#endif
