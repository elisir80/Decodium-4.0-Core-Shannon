#include <limits>

#include <QtTest>

#include "Transceiver/QmxTelemetry.hpp"

class TestQmxTelemetry final : public QObject
{
  Q_OBJECT

private slots:
  void parsesPowerInTenthsOfAWatt ()
  {
    unsigned int milliwatts = 0;
    QVERIFY (decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC19;"), &milliwatts));
    QCOMPARE (milliwatts, 1900U);

    QVERIFY (decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("  PC00;\r\n"), &milliwatts));
    QCOMPARE (milliwatts, 0U);
  }

  void parsesSwrInHundredths ()
  {
    unsigned int hundredths = 0;
    QVERIFY (decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;"), &hundredths));
    QCOMPARE (hundredths, 121U);
  }

  void rejectsEmptyReceiveModeAndMalformedReplies ()
  {
    unsigned int value = 77;
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC;"), &value));
    QCOMPARE (value, 77U);
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("PC1.9;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (QByteArrayLiteral ("SW19;"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;extra"), &value));
    QVERIFY (!decodium::qmx_telemetry::parse_swr_hundredths (QByteArrayLiteral ("SW121;"), nullptr));
  }

  void rejectsOverflow ()
  {
    unsigned int value = 0;
    QByteArray const tooLarge = QByteArrayLiteral ("PC")
        + QByteArray::number (std::numeric_limits<unsigned int>::max ()) + QByteArrayLiteral (";");
    QVERIFY (!decodium::qmx_telemetry::parse_power_milliwatts (tooLarge, &value));
  }
};

QTEST_MAIN (TestQmxTelemetry)
#include "test_qmx_telemetry.moc"
