// Fase 1 port mobile — test unitari delle regole pure del sequencer estratte
// in Sequencer/QsoSequencerRules.cpp (step A strangler). Le attese fotografano
// il comportamento storico (1.0.311/315, 1.0.437 weak boost, 1.0.289 quick
// give-up, 1.0.174 QSB fallback): se un refactor le cambia, il test deve
// fallire PRIMA che la regressione arrivi in release.
#include <QtTest>

#include "Sequencer/QsoSequencerRules.hpp"

using decodium::seq::deferredSignoffRetryCapForMode;

class TestQsoSequencerRules final : public QObject
{
  Q_OBJECT

private slots:
  void ftxUserCapIsAbsolute ()
  {
    // FT2/FT4/FT8: lo spinbox utente è un valore assoluto, conservative NON lo gonfia
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 4, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT4", 10, 127, false, false, 4, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 127, false, false, 4, 4, 3), 3);
    QCOMPARE (deferredSignoffRetryCapForMode ("ft8 ", 10, 127, false, false, 4, 4, 3), 3); // trim+upper
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -20, true, false, 4, 4, 3), 4);   // conservative ignorato
  }

  void ftxUserCapClamped ()
  {
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 0, 4, 3), 1);   // min 1
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 127, false, false, 99, 4, 3), 8);  // max 8
  }

  void weakBoostOptIn ()
  {
    // 1.0.437: weakBoost ON + partner <= soglia -> cap + bonus (clampato a 8)
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, -18, false, false, 4, 4, 3, true, -15, 3), 6);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, -14, false, false, 4, 4, 3, true, -15, 3), 3);  // sopra soglia
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 127, false, false, 4, 4, 3, true, -15, 3), 3);  // SNR ignoto
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -20, false, false, 7, 4, 3, true, -15, 6), 8);  // clamp 8
  }

  void quickGiveUpStrongOnlyReduces ()
  {
    // 1.0.289: partner forte (SNR>0) -> cap ridotto a 4, mai alzato
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, 5, false, true, 8, 4, 3), 4);
    QCOMPARE (deferredSignoffRetryCapForMode ("FT8", 10, 5, false, true, 4, 4, 2), 2);   // già sotto 4
    QCOMPARE (deferredSignoffRetryCapForMode ("FT2", 10, -5, false, true, 8, 4, 3), 8);  // partner debole: no
  }

  void legacyFallbackQsbAware ()
  {
    // Modi non-FTX: modeCap 3 + extra QSB (1.0.174) + conservative
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10), 3);
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -12), 4);          // -12 -> +1
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -18), 7);          // -18 -> +4
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, -30), 7);          // clamp +4
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, 127, true), 5);    // conservative +2
    QCOMPARE (deferredSignoffRetryCapForMode ("MSK144", 10, -18, true), 9); // 3+4+2
    QCOMPARE (deferredSignoffRetryCapForMode ("Q65", 10, 5, false, true), 3);  // strong give-up: min(3,4)
  }
};

QTEST_APPLESS_MAIN (TestQsoSequencerRules)
#include "test_qso_sequencer_rules.moc"
