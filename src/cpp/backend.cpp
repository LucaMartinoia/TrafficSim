#include <QJsonArray>
#include <QJsonObject>

#include "backend.h"

// In the constructor we connect the signals to the slots
Backend::Backend(QObject* parent) : QObject(parent)
{
  // Route successful and failed Overpass replies through named backend handlers
  QObject::connect(&m_OSMclient, &OSMClient::requestSucceeded, this, &Backend::handleOSMRequest);
  QObject::connect(&m_OSMclient, &OSMClient::requestFailed, this, &Backend::handleOSMError);
}

// Forward the UI requests
void Backend::requestBBoxData(double south, double west, double north, double east)
{
  m_OSMclient.requestRoadsBBox(south, west, north, east);
}

void Backend::handleOSMRequest(const QString& query, const QJsonDocument& response)
{
  // Prepare the simulation's network before dispatching road paths to the UI
  QString simulationError;
  if (!m_simulation.start(response, simulationError))
  {
    qWarning().noquote() << "Could not start simulation for query:" << query << simulationError;
    return;
  }

  // Prepare display paths and their summary counts for the existing success log
  const QJsonArray elements = response.object().value(QStringLiteral("elements")).toArray();
  const QVariantList paths = RoadNetwork::roadsPolyline(response);
  int geometryPoints = 0;
  for (const QVariant& path : paths)
    geometryPoints += path.toList().size();

  // Dispatch prepared paths to the map and report the processed response
  qInfo() << "Overpass request succeeded:" << elements.size() << "elements," << paths.size()
          << "ways," << geometryPoints << "geometry points.";
  emit roadsReceived(paths);
}

// Failed OSM request handling
void Backend::handleOSMError(const QString& query, const QString& error)
{
  qWarning().noquote() << "Overpass request failed for query:" << query << error;
}
