#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QSettings>

#include "backend.h"
#include "geo_utils.h"

// In the constructor we connect the signals to the slots and load settings
Backend::Backend(QObject* parent) : QObject(parent)
{
  // Register the saved coordinate type before QSettings attempts to load it
  qRegisterMetaType<QGeoCoordinate>("QGeoCoordinate");

  // Route successful and failed Overpass replies through named backend handlers
  connect(&m_OSMclient, &OSMClient::requestSucceeded, this, &Backend::handleOSMRequest);
  connect(&m_OSMclient, &OSMClient::requestFailed, this, &Backend::handleOSMError);
  // Forward simulation output and failures while keeping timer handling internal
  connect(&m_simulation, &Simulation::carsUpdated, this, &Backend::carsUpdated);
  connect(&m_simulation, &Simulation::updateFailed, this, &Backend::handleSimulationError);

  loadSettings();
}

/*
SETTINGS METHODS
*/

// Load persisted defaults
void Backend::loadSettings()
{
  QSettings settings;
  const QString locationKey = QStringLiteral("map/location");

  bool zoomOk = false;
  const QGeoCoordinate location = settings.value(locationKey).value<QGeoCoordinate>();
  const int zoom = settings.value(QStringLiteral("map/zoom"), m_defaultZoom).toInt(&zoomOk);

  if (zoomOk && GeoUtils::isValidMapView(location, zoom))
  {
    m_defaultLocation = location;
    m_defaultZoom = zoom;
  }

  // Print the effective settings after validating the saved map defaults
  qInfo() << "Loaded settings:";
  for (const QString& key : settings.allKeys())
    qInfo() << key << "=" << settings.value(key);
}

// Setter method for the persistent settings
bool Backend::saveSettings(const QGeoCoordinate& center, int zoom)
{
  // Reject invalid map defaults before persisting them
  if (!GeoUtils::isValidMapView(center, zoom))
  {
    qWarning() << "Cannot save invalid default map view";
    return false;
  }

  // Persist the validated defaults using this application's per-user settings
  QSettings settings;
  settings.setValue(QStringLiteral("map/location"), QVariant::fromValue(center));
  settings.setValue(QStringLiteral("map/zoom"), zoom);
  settings.sync();
  if (settings.status() != QSettings::NoError)
  {
    qWarning() << "Could not save default map view settings";
    return false;
  }

  // Keep the backend's in-memory defaults in sync with the persisted values
  m_defaultLocation = center;
  m_defaultZoom = zoom;
  return true;
}

// Getter method for the persistent settings
QVariantMap Backend::getSettings() const
{
  // Return the three map defaults through the QML-invokable getter
  QVariantMap mapView;
  mapView.insert(QStringLiteral("location"), QVariant::fromValue(m_defaultLocation));
  mapView.insert(QStringLiteral("zoom"), m_defaultZoom);
  return mapView;
}

// Forward cache clearing
bool Backend::clearCache() { return m_OSMclient.clearCache(); }

/*
SETTER METHODS
*/

// Apply a routing metric selected by the toolbar
bool Backend::setRoutingMetric(const QString& metric)
{
  m_simulation.roadNetwork().setRoutingMetric(metric);
  return true;
  // TODO: change bool to void
}

// Forward the toolbar's target scale to the traffic model
bool Backend::setTargetScale(double scale) { return m_simulation.traffic().setTargetScale(scale); }

// Forward the UI requests
void Backend::requestBBoxData(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  m_OSMclient.requestRoadsBBox(topLeft, bottomRight);

  // Set the simulation population from this request's bounding-box area
  m_simulation.traffic().setTargetFromBBox(topLeft, bottomRight);
}

// Reset simulation
void Backend::clearSimulation()
{
  m_OSMclient.cancelPendingRequests();
  m_simulation.clear();
  emit roadsReceived({});
  emit carsUpdated({});
}

/*
HANDLER METHODS
*/

// Valid OSM request handling
void Backend::handleOSMRequest(const QString& query, const QJsonDocument& response)
{
  // Prepare the simulation's network before dispatching road paths to the UI
  QString simulationError;
  if (!m_simulation.start(response, simulationError))
  {
    qWarning().noquote() << "Could not start simulation for query:" << query << simulationError;
    return;
  }

  // Prepare the map paths and publish the initial empty traffic state
  const QVariantList paths = RoadNetwork::roadsPolyline(response);
  emit roadsReceived(paths);
}

// Failed OSM request handling
void Backend::handleOSMError(const QString& query, const QString& error)
{
  qWarning().noquote() << "Overpass request failed for query:" << query << error;
}

// Stop and clear the simulation after a failed timer-driven update
void Backend::handleSimulationError(const QString& error)
{
  clearSimulation();
  qWarning().noquote() << "Could not update simulation:" << error;
}

void Backend::publishLogMessage(QString severity, QString message)
{
  emit logMessage(severity, message);
}
