#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "backend.h"

int main(int argc, char* argv[])
{
  // Set QML-item style
  QQuickStyle::setStyle("Basic");
  // The
  qInfo() << "--------------------\nStarting the application\n--------------------\n";
  QGuiApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("TrafficSim"));
  QCoreApplication::setApplicationName(QStringLiteral("TrafficSim"));

  // Register the backend to the QML engine
  QQmlApplicationEngine engine;
  Backend backend;
  // Pass the backend as a required root property before loading the QML module
  engine.setInitialProperties(
      {{QStringLiteral("backend"), QVariant::fromValue(static_cast<QObject*>(&backend))}});
  engine.loadFromModule("TrafficSim", "Main");
  if (engine.rootObjects().isEmpty())
    return -1;

  // Entry point
  return app.exec();
}