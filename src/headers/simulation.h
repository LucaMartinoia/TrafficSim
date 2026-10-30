#pragma once

#include <cstddef>

#include <QJsonDocument>
#include <QString>

#include "roads.h"
#include "traffic.h"

class Simulation
{
public:
  // Load and prepare a road network without starting a blocking simulation loop
  bool init(const QJsonDocument& response, QString& error);
  // Update traffic and maintain the requested active-car population for one timestep
  bool update(QString& error);
  // Reset the loaded network and simulation state
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