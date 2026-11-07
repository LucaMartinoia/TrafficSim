#include <cmath>
#include <limits>
#include <random>
#include <utility>

#include "geo_utils.h"
#include "traffic.h"

// Helper to retrive edges from the roads adjacency list
const RoadEdge& Car::getEdge(const RoadNetwork& network) const
{
  const RoadEdgeId& id = route[routeSegment];
  return network.nodes()[id.sourceNode].edges[id.edgeIndex];
}

// Add a single car to the simulation
void Traffic::addCar(const RoadNetwork& network)
{
  // Get the adjacency list
  const std::vector<RoadNode>& nodes = network.nodes();
  Car car;

  // Select distinct start and destination nodes uniformly at random
  std::uniform_int_distribution<std::size_t> nodeDistribution(0, nodes.size() - 1);
  std::size_t start = nodeDistribution(m_randomEngine);
  std::size_t destination = nodeDistribution(m_randomEngine);

  // Loop until selected nodes lead to a valid path
  while (start == destination || !network.findRoute(start, destination, car.route))
  {
    start = nodeDistribution(m_randomEngine);
    destination = nodeDistribution(m_randomEngine);
  }

  // Initialize the car at its route origin using the first referenced edge
  car.position = nodes[start].coordinate;
  car.velocity = car.getEdge(network).maxVelocity;
  m_cars.push_back(std::move(car));
}

void Traffic::createCars(const RoadNetwork& network, double dt)
{
  // Refuse invalid targets
  if (m_target <= 0)
    return;

  // Increase the arrival rate as the active population falls below its target
  // TODO: eventually scale lambda by a random factor to increase fluctuations
  constexpr double lambda0 = 0.5;
  constexpr double recoveryTime = 10.0;
  const double target = static_cast<double>(m_target) * m_targetScale;
  const double deltaN = target - static_cast<double>(m_cars.size());
  const double lambda = std::max(0.0, lambda0 + deltaN / recoveryTime);

  // If more cars than target, don't spawn new cars
  if (lambda <= 0)
    return;

  // Sample spawn number
  std::poisson_distribution<int> arrivalCount(lambda * dt);
  int arrivals = arrivalCount(m_randomEngine);

  for (int arrival = 0; arrival < arrivals; ++arrival)
    addCar(network);
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
      // TODO: add traffic flag (velocity = l/minTime without flag)
      car.velocity = edge.length / edge.travelTime;
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

void Traffic::removeCars()
{
  // Remove arrived cars using the erase-remove idiom
  std::erase_if(m_cars, [](const Car& car) { return car.arrived; });
}

/*
SETTER METHODS
*/

// Estimate population-density correction from road density
double Traffic::roadDensityFactor(int waysCount, double area)
{
  constexpr double ruralFactor = 0.1;
  constexpr double urbanFactor = 100.0;

  // Reference density: 121 ways / 211300 m²
  constexpr double urbanRoadDensity = 150.0 / 211300.0;

  if (area <= 0.0 || !std::isfinite(area) || waysCount <= 0)
    return 1;

  const double roadDensity = waysCount / area;
  const double x = std::clamp(roadDensity / urbanRoadDensity, 0.0, 1.0);

  return ruralFactor + (urbanFactor - ruralFactor) * x;
}

// Set number of cars target from view
void Traffic::setTargetFromBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox,
                                int waysCount)
{
  constexpr double peoplePerSquareMeter = 2.0e-4;
  constexpr double carsPerPerson = 0.7;
  constexpr double movingCarRatio = 0.1;

  const double area = GeoUtils::area(bbox);

  const double baseCars = area * peoplePerSquareMeter * carsPerPerson * movingCarRatio;

  const double factor = roadDensityFactor(waysCount, area);
  const double estimatedCars = baseCars * factor;

  if (!std::isfinite(estimatedCars) || estimatedCars < 0.0)
    return;

  const int target = std::max(2, static_cast<int>(estimatedCars));

  /*
    qInfo() << "Area:" << area << "Ways:" << waysCount << "Density factor:" << factor
          << "Target cars:" << target;
  */

  setTarget(target);
}

// Target setter
void Traffic::setTarget(int count) { m_target = count > 0 ? count : 0; }

// Set the target multiplier
void Traffic::setTargetScale(const double scale) { m_targetScale = scale > 0 ? scale : 1; }

/*
GETTER METHODS
*/
const std::vector<Car>& Traffic::cars() const { return m_cars; }
int Traffic::target() const { return m_target; }