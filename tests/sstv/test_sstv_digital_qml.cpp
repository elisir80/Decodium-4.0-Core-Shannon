// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/digital/HamDrmController.h"

#include <QtTest/QtTest>

#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopedPointer>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantMap>

namespace hamdrm = decodium::sstv::hamdrm;

namespace {

QString qmlErrors(const QList<QQmlError>& errors)
{
    QStringList lines;
    lines.reserve(errors.size());
    for (const QQmlError& error : errors) {
        lines.push_back(error.toString());
    }
    return lines.join(QLatin1Char('\n'));
}

QObject* requiredObject(QObject* root, const char* objectName)
{
    QObject* object = root->findChild<QObject*>(
        QString::fromLatin1(objectName));
    if (object == nullptr) {
        qWarning() << "Missing required QML object" << objectName;
    }
    return object;
}

} // namespace

class TestSstvDigitalQml final : public QObject
{
    Q_OBJECT

private slots:
    void pageCreatesExposesNamedControlsAndRendersOffscreen()
    {
        QTemporaryDir partialRoot;
        QVERIFY(partialRoot.isValid());
        hamdrm::HamDrmControllerConfig config;
        config.partialStoreRoot = partialRoot.path();
        hamdrm::HamDrmControllerBackends backends;
        backends.jpeg2000 = hamdrm::makeNativeHamDrmJpeg2000Backend();
        hamdrm::HamDrmController controller(config, std::move(backends));

        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine,
                &QQmlEngine::warnings,
                this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_DIGITAL_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QVariantMap initial;
        initial.insert(QStringLiteral("controller"),
                       QVariant::fromValue(
                           static_cast<QObject*>(&controller)));
        QScopedPointer<QObject> object(
            component.createWithInitialProperties(initial));
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* page = qobject_cast<QQuickItem*>(object.data());
        QVERIFY(page);
        QCOMPARE(page->objectName(), QStringLiteral("sstvDigitalPage"));

        const QList<const char*> expectedObjects {
            "hamdrmCapabilityMessage",
            "hamdrmProfileSelector",
            "hamdrmStartRx",
            "hamdrmCancelRx",
            "hamdrmChooseTxImage",
            "hamdrmValidateTxImage",
            "hamdrmStartTx",
            "hamdrmCancelTx",
            "hamdrmObjectInbox",
            "hamdrmResumeTransportId",
            "hamdrmResumePartial",
            "hamdrmBuildBsr",
            "hamdrmDiscardObject",
            "hamdrmBsrText",
            "hamdrmError",
        };
        for (const char* name : expectedObjects) {
            QVERIFY2(requiredObject(page, name) != nullptr, name);
        }

        QObject* selector = requiredObject(page, "hamdrmProfileSelector");
        QVERIFY(selector);
        QCOMPARE(selector->property("count").toInt(), 72);
        QCOMPARE(selector->property("currentValue").toString(),
                 controller.selectedProfileId());
        const QString capability = requiredObject(
            page, "hamdrmCapabilityMessage")->property("text").toString();
        QVERIFY(capability.contains(QStringLiteral("72 named profiles"),
                                    Qt::CaseInsensitive));
        QVERIFY(capability.contains(QStringLiteral("waveform"),
                                    Qt::CaseInsensitive));

        QSignalSpy rejected(&controller,
                            &hamdrm::HamDrmController::operationRejected);
        QVERIFY(!controller.startRx());
        QCOMPARE(rejected.size(), 1);
        QCOMPARE(rejected.front().at(0).toString(), QStringLiteral("rx"));
        QVERIFY(!requiredObject(page, "hamdrmError")
                     ->property("text").toString().isEmpty());

        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#03070a")));
        window.resize(1'000, 820);
        page->setParentItem(window.contentItem());
        page->setSize(QSizeF(1'000.0, 820.0));
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(page->window() == &window, 2'000);
        QTest::qWait(250);
        const QImage rendered = window.grabWindow();
        QVERIFY2(!rendered.isNull(),
                 "Offscreen QQuickWindow produced no HAMDRM frame");
        QCOMPARE(rendered.size(), QSize(1'000, 820));

        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 19) {
            for (int x = 0; x < rendered.width(); x += 19) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 10,
                 "Rendered HAMDRM page lacks meaningful visual content");
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
    }

    void workspaceLoadsSelectsDigitalPageAndRendersOffscreen()
    {
        QQmlEngine engine;
        QStringList runtimeWarnings;
        connect(&engine,
                &QQmlEngine::warnings,
                this,
                [&runtimeWarnings](const QList<QQmlError>& warnings) {
                    for (const QQmlError& warning : warnings) {
                        runtimeWarnings.push_back(warning.toString());
                    }
                });

        const QString sourcePath = QString::fromUtf8(
            DECODIUM_SSTV_WORKSPACE_QML_SOURCE);
        QQmlComponent component(&engine,
                                QUrl::fromLocalFile(sourcePath),
                                QQmlComponent::PreferSynchronous);
        QVERIFY2(component.isReady(), qPrintable(qmlErrors(component.errors())));
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(qmlErrors(component.errors())));
        auto* window = qobject_cast<QQuickWindow*>(object.data());
        QVERIFY(window);

        const QVariantList labels = window->property("pageLabels").toList();
        QCOMPARE(labels.size(), 7);
        QCOMPARE(labels.at(4).toString(), QStringLiteral("Digital HAMDRM"));
        QVERIFY(window->setProperty("pageIndex", 4));
        QCOMPARE(window->property("pageIndex").toInt(), 4);

        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 2'000);
        QTest::qWait(250);
        const QImage rendered = window->grabWindow();
        QVERIFY2(!rendered.isNull(),
                 "Offscreen SSTV workspace produced no frame");
        QCOMPARE(rendered.size(), QSize(1'120, 740));

        QSet<QRgb> sampledColours;
        for (int y = 0; y < rendered.height(); y += 19) {
            for (int x = 0; x < rendered.width(); x += 19) {
                sampledColours.insert(rendered.pixel(x, y));
            }
        }
        QVERIFY2(sampledColours.size() > 10,
                 "Rendered SSTV workspace lacks meaningful visual content");
        QVERIFY2(runtimeWarnings.isEmpty(),
                 qPrintable(runtimeWarnings.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestSstvDigitalQml)
#include "test_sstv_digital_qml.moc"
