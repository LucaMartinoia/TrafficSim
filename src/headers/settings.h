#pragma once

#include <QGeoCoordinate>
#include <QVariantMap>

class Settings
{
public:
  // Load the saved map defaults into the settings instance
  void loadSettings();
  bool saveSettings(const QGeoCoordinate& center, int zoom);
  QVariantMap getSettings() const;

private:
  // Keep the map defaults alongside the methods that manage them
  QGeoCoordinate m_defaultLocation{44.4056, 8.9463};
  int m_defaultZoom = 18;
};
