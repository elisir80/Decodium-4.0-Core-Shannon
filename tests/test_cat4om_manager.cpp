#include "src/radio/DecodiumCat4OmManager.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

class Cat4OmManagerTest final : public QObject
{
    Q_OBJECT

private:
    QWebSocketServer m_management {QStringLiteral("cat4om-management-test"),
                                   QWebSocketServer::NonSecureMode};
    QWebSocketServer m_control {QStringLiteral("cat4om-control-test"),
                                QWebSocketServer::NonSecureMode};
    QWebSocket* m_managementClient {nullptr};
    QWebSocket* m_controlClient {nullptr};
    QStringList m_actions;
    qint64 m_frequency {14074000};
    QString m_mode {QStringLiteral("USB")};
    bool m_ptt {false};

    QJsonObject radioState() const
    {
        return QJsonObject{
            {QStringLiteral("radioId"), QStringLiteral("ic7300")},
            {QStringLiteral("radioName"), QStringLiteral("Icom IC-7300")},
            {QStringLiteral("connectionStatus"), QStringLiteral("connected")},
            {QStringLiteral("availableVfos"), QJsonArray{QStringLiteral("VFOA"), QStringLiteral("VFOB")}},
            {QStringLiteral("activeVfo"), QStringLiteral("VFOA")},
            {QStringLiteral("txVfo"), QStringLiteral("VFOA")},
            {QStringLiteral("vfos"), QJsonObject{
                 {QStringLiteral("VFOA"), QJsonObject{
                      {QStringLiteral("frequency"), m_frequency},
                      {QStringLiteral("mode"), m_mode}}},
                 {QStringLiteral("VFOB"), QJsonObject{
                      {QStringLiteral("frequency"), 14076000},
                      {QStringLiteral("mode"), m_mode}}}}},
            {QStringLiteral("split"), false},
            {QStringLiteral("ptt"), m_ptt},
            {QStringLiteral("availableCommands"), QJsonArray{
                 QStringLiteral("SetFrequency"), QStringLiteral("SetMode"),
                 QStringLiteral("SetPtt"), QStringLiteral("SetSplit")}},
            {QStringLiteral("supportedModes"), QJsonArray{
                 QStringLiteral("USB"), QStringLiteral("DATA-U")}},
            {QStringLiteral("metering"), QJsonObject{
                 {QStringLiteral("power"), 12.5},
                 {QStringLiteral("swr"), 1.2},
                 {QStringLiteral("alc"), 17.0}}}
        };
    }

    static void send(QWebSocket* socket, QJsonObject const& object)
    {
        QVERIFY(socket);
        socket->sendTextMessage(QString::fromUtf8(
            QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }

    void sendState()
    {
        send(m_controlClient, QJsonObject{
             {QStringLiteral("type"), QStringLiteral("stateUpdate")},
             {QStringLiteral("masterId"), QStringLiteral("control-client")},
             {QStringLiteral("radios"), QJsonArray{radioState()}}});
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_management.listen(QHostAddress::LocalHost, 0));
        QVERIFY(m_control.listen(QHostAddress::LocalHost, 0));

        connect(&m_management, &QWebSocketServer::newConnection, this, [this]() {
            m_managementClient = m_management.nextPendingConnection();
            connect(m_managementClient, &QWebSocket::textMessageReceived,
                    this, [this](QString const& text) {
                QJsonObject const message = QJsonDocument::fromJson(text.toUtf8()).object();
                QString const type = message.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("hello")) {
                    send(m_managementClient, QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("managementWelcome")},
                         {QStringLiteral("endpoint"), QStringLiteral("management")},
                         {QStringLiteral("protocolVersion"), QStringLiteral("1.0.0")},
                         {QStringLiteral("clientId"), QStringLiteral("management-client")}});
                    return;
                }
                QString const action = message.value(QStringLiteral("action")).toString();
                if (type == QStringLiteral("request") && action == QStringLiteral("getServiceStatus")) {
                    send(m_managementClient, QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("response")},
                         {QStringLiteral("id"), message.value(QStringLiteral("id"))},
                         {QStringLiteral("success"), true},
                         {QStringLiteral("result"), QJsonObject{
                              {QStringLiteral("groups"), QJsonArray{QJsonObject{
                                   {QStringLiteral("id"), QStringLiteral("station")},
                                   {QStringLiteral("isRunning"), true},
                                   {QStringLiteral("controlPort"), int(m_control.serverPort())},
                                   {QStringLiteral("radios"), QJsonArray{QJsonObject{
                                        {QStringLiteral("radioId"), QStringLiteral("ic7300")}}}}}}}}}});
                }
            });
        });

        connect(&m_control, &QWebSocketServer::newConnection, this, [this]() {
            m_controlClient = m_control.nextPendingConnection();
            connect(m_controlClient, &QWebSocket::textMessageReceived,
                    this, [this](QString const& text) {
                QJsonObject const message = QJsonDocument::fromJson(text.toUtf8()).object();
                QString const type = message.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("hello")) {
                    send(m_controlClient, QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("welcome")},
                         {QStringLiteral("endpoint"), QStringLiteral("control")},
                         {QStringLiteral("protocolVersion"), QStringLiteral("1.0.0")},
                         {QStringLiteral("clientId"), QStringLiteral("control-client")},
                         {QStringLiteral("groupId"), QStringLiteral("station")},
                         {QStringLiteral("role"), QStringLiteral("slave")},
                         {QStringLiteral("radios"), QJsonArray{radioState()}}});
                    return;
                }
                if (type != QStringLiteral("request")) return;

                QString const action = message.value(QStringLiteral("action")).toString();
                m_actions.append(action);
                QJsonObject result;
                if (action == QStringLiteral("getOwnership")) {
                    result.insert(QStringLiteral("role"), QStringLiteral("master"));
                } else {
                    QJsonObject const params = message.value(QStringLiteral("params")).toObject();
                    if (action == QStringLiteral("setFrequency"))
                        m_frequency = params.value(QStringLiteral("frequency")).toInteger();
                    else if (action == QStringLiteral("setMode"))
                        m_mode = params.value(QStringLiteral("mode")).toString();
                    else if (action == QStringLiteral("setPtt"))
                        m_ptt = params.value(QStringLiteral("enabled")).toBool();
                }
                send(m_controlClient, QJsonObject{
                     {QStringLiteral("type"), QStringLiteral("response")},
                     {QStringLiteral("id"), message.value(QStringLiteral("id"))},
                     {QStringLiteral("success"), true},
                     {QStringLiteral("result"), result}});
                if (action != QStringLiteral("getOwnership")) sendState();
            });
        });
    }

    void discoveryOwnershipAndControlAreAsynchronous()
    {
        DecodiumCat4OmManager manager;
        manager.setManagementEndpoint(QStringLiteral("127.0.0.1:%1")
                                          .arg(m_management.serverPort()));
        manager.setControlEndpoint(QStringLiteral("127.0.0.1:%1")
                                       .arg(m_control.serverPort()));
        QSignalSpy connectedSpy(&manager, &DecodiumCat4OmManager::connectedChanged);

        manager.connectRig();
        QTRY_VERIFY_WITH_TIMEOUT(manager.connected(), 3000);
        QVERIFY(!connectedSpy.isEmpty());
        QCOMPARE(manager.groupId(), QStringLiteral("station"));
        QCOMPARE(manager.radioId(), QStringLiteral("ic7300"));
        QTRY_COMPARE_WITH_TIMEOUT(manager.ownershipState(), QStringLiteral("master"), 3000);

        manager.setRigFrequency(7074000);
        manager.setRigMode(QStringLiteral("DATA-U"));
        manager.setRigPtt(true);

        QTRY_COMPARE_WITH_TIMEOUT(manager.frequency(), 7074000.0, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(manager.mode(), QStringLiteral("DATA-U"), 3000);
        QTRY_VERIFY_WITH_TIMEOUT(manager.pttActive(), 3000);
        QVERIFY(m_actions.contains(QStringLiteral("getOwnership")));
        QVERIFY(m_actions.contains(QStringLiteral("setFrequency")));
        QVERIFY(m_actions.contains(QStringLiteral("setMode")));
        QVERIFY(m_actions.contains(QStringLiteral("setPtt")));

        manager.setRigPtt(false);
        QTRY_VERIFY_WITH_TIMEOUT(!manager.pttActive(), 3000);
        manager.disconnectRig();
    }
};

QTEST_GUILESS_MAIN(Cat4OmManagerTest)
#include "test_cat4om_manager.moc"
