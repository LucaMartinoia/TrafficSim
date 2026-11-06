#include <cmath>
#include <exception>
#include <limits>
#include <random>
#include <utility>

#include <QDebug>

#include "geo_utils.h"
#include "traffic.h"

// Helper to retrive edge fata from the adjacency list
const RoadEdge& Car::getEdge(const RoadNetwork& network) const
{
  const RoadEdgeId& id = route[routeSegment];
  return network.nodes()[id.sourceNode].edges[id.edgeIndex];
}

// Add a single car to the simulation
bool Traffic::addCar(const RoadNetwork& network, QString& error)
{
  // Clear a previous error because invalid routes are intentionally ignored
  error.clear();

  // Get the adjacency list
  const std::vector<RoadNode>& nodes = network.nodes();
  Car car;

  // Select distinct start and destination nodes uniformly at random
  std::uniform_int_distribution<std::size_t> nodeDistribution(0, nodes.size() - 1);
  std::size_t start = nodeDistribution(m_randomEngine);
  std::size_t destination = nodeDistribution(m_randomEngine);

  // Check if selected nodes lead to a valid path
  while (start == destination || !network.findRoute(start, destination, car.route))
  {
    start = nodeDistribution(m_randomEngine);
    destination = nodeDistribution(m_randomEngine);
  }

  // Initialize the car at its route origin using the first referenced edge
  car.position = nodes[start].coordinate;
  car.velocity = car.getEdge(network).maxVelocity;
  m_cars.push_back(std::move(car));
  return true;
}

bool Traffic::spawnCars(const RoadNetwork& network, double dt, QString& error)
{
  // Refuse invalid time steps before constructing the arrival distribution
  if (!std::isfinite(dt) || dt < 0.0)
  {
    error = QStringLiteral("Car spawning requires a finite, non-negative timestep.");
    return false;
  }

  if (m_target <= 0)
    return true;

  // Increase the arrival rate as the active population falls below its target
  // TODO: eventually scale lambda by a random factor to increase fluctuations
  constexpr double lambda0 = 0.5;
  constexpr double recoveryTime = 10.0;
  const double target = static_cast<double>(m_target) * m_targetScale;
  const double deltaN = target - static_cast<double>(m_cars.size());
  const double lambda = std::max(0.0, lambda0 + deltaN / recoveryTime);

  // If more cars than target, don't spawn new cars
  if (lambda <= 0)
  {
    return true;
  }

  // Sample spawn number
  std::poisson_distribution<int> arrivalCount(lambda * dt);
  int arrivals = arrivalCount(m_randomEngine);

  for (int arrival = 0; arrival < arrivals; ++arrival)
  {
    if (!addCar(network, error))
      return false;
  }

  return true;
}

// Update step
void Traffic::update(RoadNetwork& network, double dt)
{
  const auto& nodes = network.nodes();

  for (Car& car : m_cars)
  {
    // Time in this update
    double remainingTime = dt;

    // While there is time left and car is not arrived, move it forward
    while (remainingTime > 0.0 && car.routeSegment < car.route.size())
    {
      // Current edge data
      const RoadEdge& edge = car.getEdge(network);
      car.velocity = edge.length / edge.adaptiveTime;
      const double remainingDistance = edge.length - car.distance;
      const double timeToEnd = remainingDistance / car.velocity;

      // If enough time to reach end of this segment, move to next one
      if (remainingTime >= timeToEnd)
      {
        car.distance = 0.0;
        remainingTime -= timeToEnd;
        ++car.routeSegment;
      }
      else
      {
        car.distance += car.velocity * remainingTime;
        remainingTime = 0.0;
      }
    }

    if (car.routeSegment >= car.route.size())
      car.arrived = true;

    // Update geographic position and register occupancy
    if (!car.arrived)
    {
      const RoadEdgeId& id = car.route[car.routeSegment];
      const RoadEdge& edge = car.getEdge(network);

      const QGeoCoordinate& start = nodes[id.sourceNode].coordinate;
      const QGeoCoordinate& end = nodes[edge.destination].coordinate;

      const double azimuth = start.azimuthTo(end);
      car.position = start.atDistanceAndAzimuth(car.distance, azimuth);

      network.incrementOccupancy(id);
    }
  }
}

// TODO: maybe we can just keep the cars in the array and set the flag to false?
void Traffic::removeCars()
{
  // Remove arrived cars using the erase-remove idiom
  std::erase_if(m_cars, [](const Car& car) { return car.arrived; });
}

/*
SETTER METHODS
*/

// Set number of cars target from view
void Traffic::setTargetFromBBox(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  // Convert the bounding-box area into the estimated moving-car population
  constexpr double peoplePerSquareMeter = 2.0e-3;
  constexpr double carsPerPerson = 0.7;
  constexpr double movingCarRatio = 0.1;
  const double estimatedCars =
      GeoUtils::area(topLeft, bottomRight) * peoplePerSquareMeter * carsPerPerson * movingCarRatio;
  // TODO: remove and put a default traffic value
  if (!std::isfinite(estimatedCars) ||
      estimatedCars > static_cast<double>(std::numeric_limits<int>::max()))
    return;

  // Delegate the rounded estimate to the generic car-count setter
  setTarget(static_cast<int>(estimatedCars));
  return;
}

// Target setter
void Traffic::setTarget(int count) { m_target = count > 0 ? count : 0; }

// Set the target multiplier
bool Traffic::setTargetScale(double scale)
{
  if (!std::isfinite(scale) || scale < 0.1 || scale > 10.0)
  {
    qWarning() << "Ignoring invalid traffic scale:" << scale;
    return false;
  }

  m_targetScale = scale;
  return true;
}

/*
GETTER METHODS
*/
const std::vector<Car>& Traffic::cars() const { return m_cars; }
int Traffic::target() const { return m_target; }