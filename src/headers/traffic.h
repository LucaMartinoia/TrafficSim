#pragma once

#include <cstddef>
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
  // Pick random endpoints and add a car only when route planning succeeds
  bool addCar(const RoadNetwork& network, QString& error);
  void removeCars();

  // Advance all cars against the same occupancy snapshot for this timestep
  void update(RoadNetwork& network, double dt);

  std::size_t activeCarCount() const;
  const std::vector<Car>& cars() const;

private:
  std::vector<Car> m_cars;
};
