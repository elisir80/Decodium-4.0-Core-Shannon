#include "GraphicsPipelineCache.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeFile(const QString& path, const QByteArray& payload)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(payload) == payload.size();
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QString readSource(const QString& relativePath)
{
    QFile file(QStringLiteral(DECODIUM_SOURCE_DIR "/") + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestGraphicsPipelineCache final : public QObject
{
    Q_OBJECT

private slots:
    void cacheNamesInvalidateTheLegacyFormatAndSeparateRoles();
    void interruptedSessionDiscardsPersistentAndStagedCaches();
    void cleanSessionPromotesTheStageAtomically();
    void softwareRendererRecoverySwitchWorksOutsideWindows();
};

void TestGraphicsPipelineCache::cacheNamesInvalidateTheLegacyFormatAndSeparateRoles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QString const bootstrap = decodium::graphics_cache::stableCacheFile(
        directory.path(), QStringLiteral("bootstrap"));
    QString const main = decodium::graphics_cache::stableCacheFile(
        directory.path(), QStringLiteral("main"));

    QVERIFY(bootstrap.contains(QStringLiteral("qsg_pipeline_cache_v2_")));
    QVERIFY(bootstrap.endsWith(QStringLiteral(".bin")));
    QVERIFY(bootstrap != main);
    QVERIFY(decodium::graphics_cache::stagingCacheFile(bootstrap)
                .startsWith(bootstrap + QStringLiteral(".stage.")));
}

void TestGraphicsPipelineCache::interruptedSessionDiscardsPersistentAndStagedCaches()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString stable;
    QString staged;

    {
        decodium::graphics_cache::PipelineCacheSession interrupted(directory.path());
        QVERIFY(interrupted.markerArmed());
        stable = decodium::graphics_cache::stableCacheFile(
            directory.path(), QStringLiteral("main"));
        staged = decodium::graphics_cache::stagingCacheFile(stable);
        QVERIFY(writeFile(stable, QByteArrayLiteral("last-known-cache")));
        QVERIFY(writeFile(staged, QByteArrayLiteral("incomplete-cache")));
        // Deliberately do not mark this session clean, as if the compositor or
        // process had frozen during scene-graph teardown.
    }

    QVERIFY(QFileInfo::exists(stable));
    QVERIFY(QFileInfo::exists(staged));
    {
        decodium::graphics_cache::PipelineCacheSession recovered(directory.path());
        QVERIFY(recovered.recoveredFromInterruptedSession());
        QVERIFY(recovered.removedFileCount() >= 2);
        QVERIFY(!decodium::graphics_cache::pipelineCacheLoadAllowed());
        QVERIFY(!QFileInfo::exists(stable));
        QVERIFY(!QFileInfo::exists(staged));
        recovered.markCleanShutdown();
    }
}

void TestGraphicsPipelineCache::cleanSessionPromotesTheStageAtomically()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QString stable;
    QString staged;

    {
        decodium::graphics_cache::PipelineCacheSession session(directory.path());
        QVERIFY(session.markerArmed());
        stable = decodium::graphics_cache::stableCacheFile(
            directory.path(), QStringLiteral("main"));
        staged = decodium::graphics_cache::stagingCacheFile(stable);
        QVERIFY(writeFile(stable, QByteArrayLiteral("old-cache")));
        QVERIFY(writeFile(staged, QByteArrayLiteral("new-cache")));
        session.markCleanShutdown();
    }

    QCOMPARE(readFile(stable), QByteArrayLiteral("new-cache"));
    QVERIFY(!QFileInfo::exists(staged));
    QDir const directoryView(directory.path());
    QVERIFY(directoryView.entryList(
                QStringList {decodium::graphics_cache::cachePrefix()
                             + QStringLiteral("*session.pending")},
                QDir::Files).isEmpty());
}

void TestGraphicsPipelineCache::softwareRendererRecoverySwitchWorksOutsideWindows()
{
    QString const source = readSource(QStringLiteral("src/app/main_qml.cpp"));
    QVERIFY(!source.isEmpty());
    QVERIFY(source.contains(QStringLiteral(
        "hasCommandLineSwitchAny(argc, argv, \"--disable-gpu\")")));
    QVERIFY(source.contains(QStringLiteral(
        "hasCommandLineSwitchAny(argc, argv, \"--software-renderer\")")));
    QVERIFY(source.contains(QStringLiteral(
        "qputenv(\"QT_QUICK_BACKEND\", \"software\")")));

    int const switchPosition = source.indexOf(QStringLiteral(
        "bool const commandLineSoftwareGraphics"));
    int const applicationPosition = source.indexOf(QStringLiteral(
        "DecodiumApplication app(argc, argv)"));
    QVERIFY(switchPosition >= 0);
    QVERIFY(applicationPosition > switchPosition);
}

QTEST_MAIN(TestGraphicsPipelineCache)
#include "test_graphics_pipeline_cache.moc"
