#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "tftpclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    app.setOrganizationName("VoIP");
    app.setOrganizationDomain("Comms");

    QQmlApplicationEngine engine;
    QQmlContext *context = engine.rootContext();
    if (nullptr != context) {
        TftpClient *client = new TftpClient();
        context->setContextProperty(client->objectName(), client);
    }

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
