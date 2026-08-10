#include "src/services/DxccLookup.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DxccLookupTest final : public QObject
{
    Q_OBJECT

private slots:
    void kg4UsesLengthSensitiveAssignment()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = directory.filePath(QStringLiteral("cty.dat"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(
            "United States: 05: 08: NA: 37.60: 91.87: 5.0: K:\n"
            "    K;\n"
            "Guantanamo Bay: 08: 11: NA: 20.00: 75.00: 5.0: KG4:\n"
            "    KG4,=KG4AC;\n");
        file.close();

        DxccLookup lookup;
        QVERIFY(lookup.loadCtyDat(path));

        // Generic one- and three-character suffixes are United States.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4A")).name,
                 QStringLiteral("United States"));
        QCOMPARE(lookup.lookup(QStringLiteral("KG4ABC")).name,
                 QStringLiteral("United States"));

        // A generic two-character suffix remains Guantanamo Bay.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4AB")).name,
                 QStringLiteral("Guantanamo Bay"));

        // Exact cty.dat exceptions remain authoritative.
        QCOMPARE(lookup.lookup(QStringLiteral("KG4AC")).name,
                 QStringLiteral("Guantanamo Bay"));
    }
};

QTEST_MAIN(DxccLookupTest)
#include "test_dxcc_lookup.moc"
