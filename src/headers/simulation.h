#pragma once

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include "roads.h"
#include "traffic.h"

class Simulation
{
public:
  // Constructor
  explicit Simulation();
  // Load and prepare a road network without starting a blocking simulation loop
  bool start(const QJsonDocument& response, QString& error);
  // Update traffic and sample new-car arrivals for one timestep
  bool update(QString& error);
  // Return active car positions for the map overlay
  QVariantList carPositions() const;
  // Reset the loaded network and simulation state
  void clear();

  // Getter methods
  double timeStep() const;
  double time() const;
  QTimer& timer();
  Traffic& traffic();

private:
  RoadNetwork m_roadNetwork;
  Traffic m_traffic;
  QTimer m_timer;
  const double m_timeStep = 0.05;
  double m_time = 0.0;
  bool m_started = false;
};