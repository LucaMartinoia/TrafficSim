#pragma once

#include <random>
#include <utility>
#include <vector>

#include <QGeoCoordinate>

#include "roads.h"

struct Car
{
  RoadRoute route;
  std::size_t routeSegment = 0;
  double distance = 0.0;
  QGeoCoordinate position;
  double velocity = 0.0;
  bool arrived = false;

  // Helper to retrive edges from the adjacency list
  const RoadEdge& getEdge(const RoadNetwork& network) const;
};

class Traffic
{
public:
  // Sample and add new cars
  void createCars(const RoadNetwork& network, double dt);
  // Add a single car
  void addCar(const RoadNetwork& network);
  void removeCars();
  // Advance all cars
  void update(RoadNetwork& network, double dt);

  // Setter methods
  void setTargetFromBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox, int waysCount);
  void setTargetScale(const double scale);

  // Getter methods
  const std::vector<Car>& cars() const;
  int target() const;

private:
  void setTarget(int count);
  double roadDensityFactor(int waysCount, double area);

  std::mt19937 m_randomEngine{std::random_device{}()};
  std::vector<Car> m_cars;
  int m_target = 1;
  double m_targetScale = 1.0;
};
