#include <QGuiApplication>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCursor>
#include <QTimer>
#include <QQuickStyle>

#include "WiFiNetworksList.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle("Basic"); // Force a customizable style

    QGuiApplication app(argc, argv);

    qint32 fontId = QFontDatabase::addApplicationFont(":/fonts/Junicode.ttf");
    QStringList fontList = QFontDatabase::applicationFontFamilies(fontId);
    if (!fontList.isEmpty()) {
        //set it as the default font for the application
        QGuiApplication::setFont(QFont(fontList.first()));
    }

    // Hide the cursor using a blank cursor
    //app.setOverrideCursor(QCursor(Qt::BlankCursor));

    WiFiNetworksList wifiNetworksList;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("wifiNetworksList", &wifiNetworksList);

    engine.addImportPath("qrc:/qmls/");
    const QUrl url(QStringLiteral("qrc:/qmls/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
