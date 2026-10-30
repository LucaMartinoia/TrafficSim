#pragma once

#include <QGeoCoordinate>
#include <QObject>
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
  Q_INVOKABLE bool clearOSMCache();
  Q_INVOKABLE bool setDefaultMapView(const QGeoCoordinate& center, int zoomLevel);
  Q_INVOKABLE QVariantMap getDefaultMapView() const;

signals:
  // Signals are Qt main async mechanism to send callbacks between modules and GUI
  void roadsReceived(const QVariantList& paths);

private slots:
  // Slots are the callbacks connected to signals
  void handleOSMRequest(const QString& query, const QJsonDocument& response);
  void handleOSMError(const QString& query, const QString& error);

private:
  void loadDefaultMapView();

  // Backend owns the OSMClient and the Simulation engine
  OSMClient m_OSMclient;
  Simulation m_simulation;
  QGeoCoordinate m_defaultLocation{44.4056, 8.9463};
  int m_defaultZoom = 18;
};
