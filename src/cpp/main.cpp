#include <QDebug>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "overpassclient.h"

int main(int argc, char* argv[])
{
  QQuickStyle::setStyle("Basic");
  QGuiApplication app(argc, argv);
  qInfo() << "Starting the application";
  QCoreApplication::setOrganizationName(QStringLiteral("TrafficSim"));
  QCoreApplication::setApplicationName(QStringLiteral("TrafficSim"));

  OverpassClient overpassClient;
  QObject::connect(
      &overpassClient, &OverpassClient::requestSucceeded,
      [](const QString&, const QJsonDocument& response)
      {
        const QJsonArray elements = response.object().value(QStringLiteral("elements")).toArray();
        int ways = 0;
        int geometryPoints = 0;
        for (const QJsonValue& elementValue : elements)
        {
          const QJsonObject element = elementValue.toObject();
          if (element.value(QStringLiteral("type")).toString() != QStringLiteral("way"))
            continue;

          ++ways;
          geometryPoints += element.value(QStringLiteral("geometry")).toArray().size();
        }

        qInfo() << "Overpass request succeeded:" << elements.size() << "elements," << ways
                << "ways," << geometryPoints << "geometry points.";
      });
  QObject::connect(&overpassClient, &OverpassClient::requestFailed,
                   [](const QString&, const QString& error)
                   { qWarning().noquote() << "Overpass request failed:" << error; });
  overpassClient.requestDefaultRoads();

  QQmlApplicationEngine engine;
  engine.loadFromModule("TrafficSim", "Main");
  // Backend backend;
  // engine.rootContext()->setContextProperty("backend", &backend);

  return app.exec();
}