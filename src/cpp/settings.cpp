#include <QSettings>

#include "geo_utils.h"
#include "settings.h"

// Load persisted defaults
void Settings::loadSettings()
{
  QSettings settings;

  bool latOk = false;
  bool lonOk = false;
  bool zoomOk = false;

  const double lat = settings.value(QStringLiteral("map/latitude")).toDouble(&latOk);
  const double lon = settings.value(QStringLiteral("map/longitude")).toDouble(&lonOk);
  const int zoom = settings.value(QStringLiteral("map/zoom"), m_defaultZoom).toInt(&zoomOk);

  const QGeoCoordinate location(lat, lon);

  if (latOk && lonOk && zoomOk && GeoUtils::isValidMapView(location, zoom))
  {
    m_defaultLocation = location;
    m_defaultZoom = zoom;

    // Print the effective settings after validating the saved map defaults
    qInfo() << "Settings loaded";
  }
  else
  {
    qWarning() << "Failed to load settings at startup: using default values";
  }
}

// Setter method for the persistent settings
bool Settings::saveSettings(const QGeoCoordinate& center, int zoom)
{
  // Reject invalid map defaults before persisting them
  if (!GeoUtils::isValidMapView(center, zoom))
  {
    qWarning() << "Cannot save invalid map view";
    return false;
  }

  // Persist the validated defaults using this application's per-user settings
  QSettings settings;
  settings.setValue(QStringLiteral("map/latitude"), center.latitude());
  settings.setValue(QStringLiteral("map/longitude"), center.longitude());
  settings.setValue(QStringLiteral("map/zoom"), zoom);
  settings.sync();

  if (settings.status() != QSettings::NoError)
  {
    qWarning() << "Cannot save map view in settings";
    return false;
  }

  // Keep the in-memory defaults in sync with the persisted values
  m_defaultLocation = center;
  m_defaultZoom = zoom;
  return true;
}

// Getter method for the persistent settings
QVariantMap Settings::getSettings() const
{
  // Return the three map defaults through the settings getter
  QVariantMap mapView;
  mapView.insert(QStringLiteral("location"), QVariant::fromValue(m_defaultLocation));
  mapView.insert(QStringLiteral("zoom"), m_defaultZoom);
  return mapView;
}
