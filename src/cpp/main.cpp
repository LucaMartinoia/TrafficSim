#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [](const QList<QQmlError> &warnings)
                     {
                         for (const QQmlError &warning : warnings)
                             qWarning() << "QML warning:" << warning.toString();
                     });

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     []()
                     { qWarning() << "Object creation failed!"; });

    engine.loadFromModule("TrafficSim", "Main");

    if (engine.rootObjects().isEmpty())
    {
        qWarning() << "No root objects created — exiting.";
        return -1;
    }

    return app.exec();
}