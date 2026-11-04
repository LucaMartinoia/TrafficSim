#pragma once

#include <QGeoCoordinate>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "OSMclient.h"
#include "simulation.h"

class Backend : public QObject
{
  // Qt macro to initialize QObjects
  Q_OBJECT

public:
  // Constructor
  explicit Backend(QObject* parent = nullptr);

  // Q_INVOKABLE make a method callable from QML's JavaScript
  Q_INVOKABLE void requestBBoxData(const QGeoCoordinate& topLeft,
                                   const QGeoCoordinate& bottomRight);
  Q_INVOKABLE void clearSimulation();
  Q_INVOKABLE bool clearCache();
  Q_INVOKABLE bool saveSettings(const QGeoCoordinate& center, int zoomLevel);
  Q_INVOKABLE QVariantMap getSettings() const;

signals:
  // Signals are Qt main async mechanism to send callbacks between modules and GUI
  void roadsReceived(const QVariantList& paths);
  void carsUpdated(const QVariantList& positions);

private slots:
  // Slots are the callbacks connected to signals
  void handleOSMRequest(const QString& query, const QJsonDocument& response);
  void handleOSMError(const QString& query, const QString& error);
  void updateSimulation();

private:
  void loadSettings();

  // Backend owns the OSMClient and the Simulation engine
  OSMClient m_OSMclient;
  Simulation m_simulation;
  QGeoCoordinate m_defaultLocation{44.4056, 8.9463};
  int m_defaultZoom = 18;
};
