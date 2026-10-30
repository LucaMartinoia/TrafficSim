#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

#include <cmath>

#include "backend.h"
#include "geo_utils.h"

// In the constructor we connect the signals to the slots and load settings
Backend::Backend(QObject* parent) : QObject(parent)
{
  // Route successful and failed Overpass replies through named backend handlers
  QObject::connect(&m_OSMclient, &OSMClient::requestSucceeded, this, &Backend::handleOSMRequest);
  QObject::connect(&m_OSMclient, &OSMClient::requestFailed, this, &Backend::handleOSMError);

  loadDefaultMapView();
}

// Load persisted defaults
void Backend::loadDefaultMapView()
{
  QSettings settings;
  bool zoomOk = false;
  const QGeoCoordinate location =
      settings.value(QStringLiteral("map/location")).value<QGeoCoordinate>();
  const int zoom = settings.value(QStringLiteral("map/zoom"), m_defaultZoom).toInt(&zoomOk);

  if (zoomOk && GeoUtils::isValidMapView(location, zoom))
  {
    m_defaultLocation = location;
    m_defaultZoom = zoom;
  }
  else if (settings.contains(QStringLiteral("map/location")) ||
           settings.contains(QStringLiteral("map/zoom")))
  {
    qWarning() << "Ignoring invalid saved map defaults";
  }
}

// Forward the UI requests
void Backend::requestBBoxData(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  m_OSMclient.requestRoadsBBox(topLeft, bottomRight);
}

// Reset simulation
void Backend::clearSimulation()
{
  m_simulation.clear();
  emit roadsReceived({});
}

// Setter method for the persistent settings
bool Backend::setDefaultMapView(const QGeoCoordinate& center, int zoom)
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
QVariantMap Backend::getDefaultMapView() const
{
  // Return the three map defaults through the QML-invokable getter
  QVariantMap mapView;
  mapView.insert(QStringLiteral("location"), QVariant::fromValue(m_defaultLocation));
  mapView.insert(QStringLiteral("zoom"), m_defaultZoom);
  return mapView;
}

bool Backend::clearOSMCache() { return m_OSMclient.clearCache(); }

// Valid OSM request handling
void Backend::handleOSMRequest(const QString& query, const QJsonDocument& response)
{
  // Prepare the simulation's network before dispatching road paths to the UI
  QString simulationError;
  if (!m_simulation.init(response, simulationError))
  {
    qWarning().noquote() << "Could not start simulation for query:" << query << simulationError;
    return;
  }

  // Prepare display paths and their summary counts for the existing success log
  const QVariantList paths = RoadNetwork::roadsPolyline(response);

  emit roadsReceived(paths);
}

// Failed OSM request handling
void Backend::handleOSMError(const QString& query, const QString& error)
{
  qWarning().noquote() << "Overpass request failed for query:" << query << error;
}
