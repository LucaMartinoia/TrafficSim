#pragma once

#include <QObject>
#include <QVariantList>

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
  Q_INVOKABLE void requestBBoxData(double south, double west, double north, double east);

signals:
  // Signals are Qt main async mechanism to send callbacks between modules and GUI
  void roadsReceived(const QVariantList& paths);

private slots:
  // Slots are the callbacks connected to signals
  void handleOSMRequest(const QString& query, const QJsonDocument& response);
  void handleOSMError(const QString& query, const QString& error);

private:
  // Backend owns the OSMClient and the Simulation engine
  OSMClient m_OSMclient;
  Simulation m_simulation;
};
