#include <random>
#include <utility>

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
  std::random_device randomDevice;
  std::mt19937 generator(randomDevice());
  std::uniform_int_distribution<std::size_t> nodeDistribution(0, nodes.size() - 1);
  std::size_t start = nodeDistribution(generator);
  std::size_t destination = nodeDistribution(generator);

  // Check if selected nodes lead to a valid path
  while (start == destination || !network.findRoute(start, destination, car.route))
  {
    start = nodeDistribution(generator);
    destination = nodeDistribution(generator);
  }

  // Initialize the car at its route origin using the first referenced edge
  car.position = nodes[start].coordinate;
  car.velocity = car.getEdge(network).maxVelocity;
  m_cars.push_back(std::move(car));
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

// Getter methods
const std::vector<Car>& Traffic::cars() const { return m_cars; }
