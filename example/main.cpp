// SPDX-License-Identifier: MIT
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml/qqmlextensionplugin.h>

// QtQuickAutofill is a static QML module
Q_IMPORT_QML_PLUGIN(QtQuickAutofillPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("AutofillExample", "Main");
    return app.exec();
}
