#pragma once

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "roads.h"
#include "traffic.h"

class Simulation : public QObject
{
  Q_OBJECT

public:
  // Constructor
  explicit Simulation(QObject* parent = nullptr);
  // Load and prepare a road network without starting a blocking simulation loop
  bool start(const QJsonDocument& response, QString& error);
  // Return active car positions for the map overlay
  QVariantList carPositions() const;
  // Reset the loaded network and simulation state
  void clear();

  // Getter methods
  double timeStep() const;
  double time() const;
  Traffic& traffic();
  RoadNetwork& roadNetwork();

public slots:
  // Advance traffic and publish the update outcome
  void update();

signals:
  void carsUpdated(const QVariantList& positions);
  void updateFailed(const QString& error);

private:
  RoadNetwork m_roadNetwork;
  Traffic m_traffic;
  QTimer m_timer;
  const double m_timeStep = 0.05;
  double m_time = 0.0;
  bool m_started = false;
};