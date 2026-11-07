#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>

#include <utility>

#include "backend.h"

// In the constructor we connect the signals to the slots and load settings
Backend::Backend(QObject* parent) : QObject(parent)
{
  // Route successful Overpass replies through the named backend handler
  connect(&m_OSMclient, &OSMClient::requestSucceeded, this, &Backend::handleOSMRequest);
  // Forward simulation state updates through the backend interface exposed to QML
  connect(&m_simulation, &Simulation::roadsReceived, this, &Backend::roadsReceived);
  connect(&m_simulation, &Simulation::carsUpdated, this, &Backend::carsUpdated);

  loadSettings();
}

// Forward settings loading to the owned settings instance
void Backend::loadSettings() { m_settings.loadSettings(); }

// Forward settings saving to the owned settings instance
bool Backend::saveSettings(const QGeoCoordinate& center, int zoomLevel)
{
  return m_settings.saveSettings(center, zoomLevel);
}

// Forward settings retrieval to the owned settings instance
QVariantMap Backend::getSettings() const { return m_settings.getSettings(); }

// Forward cache clearing
bool Backend::clearCache() { return m_OSMclient.clearCache(); }

/*
SETTER METHODS
*/

// Apply a routing metric selected by the toolbar
void Backend::setRoutingMetric(const QString& metric)
{
  m_simulation.roadNetwork().setRoutingMetric(metric);
}

// Forward the toolbar's target scale to the traffic model
void Backend::setTargetScale(const double scale) { m_simulation.traffic().setTargetScale(scale); }

// Forward the UI requests
void Backend::requestBBoxData(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  const std::pair<QGeoCoordinate, QGeoCoordinate> bbox{topLeft, bottomRight};
  m_simulation.setBBox(bbox);
  m_OSMclient.requestRoadsBBox(bbox);
}

// Reset simulation
void Backend::clearSimulation()
{
  m_OSMclient.cancelPendingRequests();
  m_simulation.clear();
}

/*
HANDLER METHODS
*/

// Start the simulation with the successful OSM response
void Backend::handleOSMRequest(const QJsonDocument& response)
{
  // Let the simulation publish the loaded network to observers
  m_simulation.start(response);
}

void Backend::publishLogMessage(QString severity, QString message)
{
  emit logMessage(severity, message);
}
