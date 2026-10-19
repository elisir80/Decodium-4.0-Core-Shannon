#include <QFile>
#include <QtTest>

namespace {

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestCwCatSpeedFallback final : public QObject
{
    Q_OBJECT

private slots:
    void unavailableSpeedControlDoesNotBlockMorseSend();
    void yaesuSingleCharacterUsesLiteralTextPath();
};

void TestCwCatSpeedFallback::unavailableSpeedControlDoesNotBlockMorseSend()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("QString HamlibTransceiver::send_morse_checked"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void HamlibTransceiver::send_morse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    QVERIFY(implementation.contains(QStringLiteral("rc == -RIG_ENAVAIL")));
    QVERIFY(implementation.contains(QStringLiteral("rc == -RIG_ENIMPL")));
    QVERIFY(implementation.contains(QStringLiteral("using the radio's configured speed")));

    const int optionalBranch = implementation.indexOf(
        QStringLiteral("if (rc == -RIG_ENAVAIL || rc == -RIG_ENIMPL)"));
    const int morseSend = implementation.indexOf(
        QStringLiteral("int const rc = rig_send_morse"));
    QVERIFY(optionalBranch >= 0);
    QVERIFY(morseSend > optionalBranch);
}

void TestCwCatSpeedFallback::yaesuSingleCharacterUsesLiteralTextPath()
{
    const QString source = readSource(QStringLiteral("Transceiver/HamlibTransceiver.cpp"));
    QVERIFY(!source.isEmpty());

    const int functionStart = source.indexOf(
        QStringLiteral("QString HamlibTransceiver::send_morse_checked"));
    const int functionEnd = source.indexOf(
        QStringLiteral("void HamlibTransceiver::send_morse"), functionStart);
    QVERIFY(functionStart >= 0);
    QVERIFY(functionEnd > functionStart);

    const QString implementation = source.mid(functionStart, functionEnd - functionStart);
    QVERIFY(implementation.contains(QStringLiteral("is_yaesu_rig")));
    QVERIFY(implementation.contains(QStringLiteral("literal_single_char")));
    QVERIFY(implementation.contains(QStringLiteral("payload += QLatin1Char (' ');")));
    const int normalization = implementation.indexOf(
        QStringLiteral("payload += QLatin1Char (' ');"));
    const int morseSend = implementation.indexOf(
        QStringLiteral("int const rc = rig_send_morse"));
    QVERIFY(normalization >= 0);
    QVERIFY(morseSend > normalization);
}

QTEST_APPLESS_MAIN(TestCwCatSpeedFallback)
#include "test_cw_cat_speed_fallback.moc"
