#pragma once

#include <cstddef>
#include <vector>

#include <QGeoCoordinate>
#include <QString>

#include "roads.h"

struct TrafficCar
{
  // Store segment progress in meters and both speeds in meters per second.
  std::vector<std::size_t> routeNodeIndices;
  std::size_t currentRouteSegment = 0;
  double distanceAlongSegment = 0.0;
  QGeoCoordinate startPosition;
  QGeoCoordinate endPosition;
  QGeoCoordinate currentPosition;
  double velocity = 0.0;
  double desiredVelocity = 0.0;
  bool arrived = false;
};

class Traffic
{
public:
  // Add a car along an explicit directed route; route finding is intentionally separate.
  // TODO: do we need to pass desiredVelocity and routeNodeIndices??
  // desiredVelocity is in meters per second.
  bool addCar(const RoadNetwork& network, const std::vector<std::size_t>& routeNodeIndices,
              double desiredVelocity, QString& error);

  // Advance all cars by simulation time in seconds, respecting each road's speed limit.
  // TODO: make dt a class member of a Simulation class or something
  bool advance(const RoadNetwork& network, double deltaTime, QString& error);

  void clear();
  const std::vector<TrafficCar>& cars() const;

private:
  std::vector<TrafficCar> m_cars;
};
