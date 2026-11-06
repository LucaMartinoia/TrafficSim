#pragma once

#include <random>
#include <vector>

#include <QGeoCoordinate>
#include <QString>

#include "roads.h"

struct Car
{
  RoadRoute route;
  std::size_t routeSegment = 0;
  double distance = 0.0;
  QGeoCoordinate position;
  double velocity = 0.0;
  bool arrived = false;

  // Helper to retrive edge fata from the adjacency list
  const RoadEdge& getEdge(const RoadNetwork& network) const;
};

class Traffic
{
public:
  // Sample and add new cars for one timestep based on the population deficit
  bool spawnCars(const RoadNetwork& network, double dt, QString& error);
  // Pick random endpoints and add a car only when route planning succeeds
  bool addCar(const RoadNetwork& network, QString& error);
  void removeCars();
  // Advance all cars against the same occupancy snapshot for this timestep
  void update(RoadNetwork& network, double dt);

  // Setter methods
  void setTargetFromBBox(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight);
  void setTarget(int count);
  // Scale the effective target without changing the bounding-box target
  bool setTargetScale(double scale);

  // Getter methods
  const std::vector<Car>& cars() const;
  int target() const;

private:
  std::mt19937 m_randomEngine{std::random_device{}()};
  std::vector<Car> m_cars;
  int m_target = 1;
  double m_targetScale = 1.0;
};
