#pragma once

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "OSMclient.h"
#include "settings.h"
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
  Q_INVOKABLE void setRoutingMetric(const QString& metric);
  Q_INVOKABLE void setTargetScale(const double scale);

signals:
  // Signals are Qt main async mechanism to send callbacks between modules and GUI
  void roadsReceived(const QVariantList& paths);
  void carsUpdated(const QVariantList& positions);
  void simulationStatus(int activeCarCount, double elapsedSeconds);
  void logMessage(QString severity, QString message);

private slots:
  // Slots are the callbacks connected to signals
  void handleOSMRequest(const QJsonDocument& response);
  void publishLogMessage(QString severity, QString message);

private:
  void loadSettings();

  // Backend owns the OSMClient and the Simulation engine
  OSMClient m_OSMclient;
  Simulation m_simulation;
  Settings m_settings;
};
