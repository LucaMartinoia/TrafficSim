#pragma once

#include <QJsonDocument>
#include <QString>

#include "roads.h"
#include "traffic.h"

class Simulation
{
public:
  // Load and prepare a road network without starting a blocking simulation loop
  bool start(const QJsonDocument& response, QString& error);
  // Advance traffic once by the configured time step and integrate simulation time
  bool step(QString& error);
  void clear();

  // Getter methods
  double timeStep() const;
  double time() const;

private:
  RoadNetwork m_roadNetwork;
  Traffic m_traffic;
  const double m_timeStep = 1.0 / 60.0;
  double m_time = 0.0;
  bool m_started = false;
};