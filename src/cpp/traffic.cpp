#include <algorithm>
#include <cmath>

#include "geo_utils.h"
#include "traffic.h"

namespace
{
const RoadEdge* findRoadEdge(const RoadNode& node, std::size_t destination)
{
  // Find the directed edge that connects this route step.
  for (const RoadEdge& edge : node.edges)
  {
    if (edge.destination == destination)
      return &edge;
  }
  return nullptr;
}

} // namespace

bool Traffic::addCar(const RoadNetwork& network, const std::vector<std::size_t>& routeNodeIndices,
                     double desiredVelocity, QString& error)
{
  // Clear the previous error before validating the new car.
  error.clear();

  const std::vector<RoadNode>& nodes = network.nodes();
  if (routeNodeIndices.size() < 2)
  {
    error = QStringLiteral("A car route must contain at least two nodes.");
    return false;
  }
  if (!std::isfinite(desiredVelocity) || desiredVelocity <= 0.0)
  {
    error = QStringLiteral("Car velocity must be a finite value greater than zero.");
    return false;
  }

  // Validate the complete route as a sequence of directed road edges.
  for (std::size_t routeIndex = 0; routeIndex < routeNodeIndices.size(); ++routeIndex)
  {
    if (routeNodeIndices[routeIndex] >= nodes.size())
    {
      error = QStringLiteral("Car route references a node outside the road network.");
      return false;
    }
    if (routeIndex > 0 &&
        !findRoadEdge(nodes[routeNodeIndices[routeIndex - 1]], routeNodeIndices[routeIndex]))
    {
      error = QStringLiteral("Car route contains a road segment that cannot be traversed.");
      return false;
    }
  }

  // Initialize the car at its route origin with coordinates taken from the network.
  const RoadNode& startNode = nodes[routeNodeIndices.front()];
  const RoadNode& endNode = nodes[routeNodeIndices.back()];
  TrafficCar car;
  car.routeNodeIndices = routeNodeIndices;
  car.startPosition = startNode.coordinate;
  car.endPosition = endNode.coordinate;
  car.currentPosition = car.startPosition;
  car.desiredVelocity = desiredVelocity;
  const RoadEdge* firstEdge = findRoadEdge(startNode, routeNodeIndices[1]);
  car.velocity = std::min(desiredVelocity, firstEdge->maxVelocity);
  m_cars.push_back(car);
  return true;
}

bool Traffic::advance(const RoadNetwork& network, double deltaTime, QString& error)
{
  // Validate the time step and every active route before changing any car state.
  error.clear();
  if (!std::isfinite(deltaTime) || deltaTime < 0.0)
  {
    error = QStringLiteral("Simulation time step must be finite and non-negative.");
    return false;
  }

  const std::vector<RoadNode>& nodes = network.nodes();
  for (const TrafficCar& car : m_cars)
  {
    if (car.arrived)
      continue;
    if (car.routeNodeIndices.size() < 2 ||
        car.currentRouteSegment + 1 >= car.routeNodeIndices.size())
    {
      error = QStringLiteral("Car has an invalid current route segment.");
      return false;
    }

    for (std::size_t routeIndex = car.currentRouteSegment;
         routeIndex + 1 < car.routeNodeIndices.size(); ++routeIndex)
    {
      const std::size_t source = car.routeNodeIndices[routeIndex];
      const std::size_t destination = car.routeNodeIndices[routeIndex + 1];
      if (source >= nodes.size() || destination >= nodes.size() ||
          !findRoadEdge(nodes[source], destination))
      {
        error = QStringLiteral("Car route no longer matches the road network.");
        return false;
      }
    }
  }

  // Spend the time step across as many route segments as the car can traverse.
  for (TrafficCar& car : m_cars)
  {
    if (car.arrived)
      continue;

    double remainingTime = deltaTime;
    while (car.currentRouteSegment + 1 < car.routeNodeIndices.size())
    {
      const std::size_t sourceIndex = car.routeNodeIndices[car.currentRouteSegment];
      const std::size_t destinationIndex = car.routeNodeIndices[car.currentRouteSegment + 1];
      const RoadEdge* edge = findRoadEdge(nodes[sourceIndex], destinationIndex);
      const RoadNode& sourceNode = nodes[sourceIndex];
      const RoadNode& destinationNode = nodes[destinationIndex];
      const QGeoCoordinate& sourcePosition = sourceNode.coordinate;
      const QGeoCoordinate& destinationPosition = destinationNode.coordinate;
      const double segmentLength = edge->length;

      // Limit the requested meters-per-second speed to the segment's speed limit.
      car.velocity = std::min(car.desiredVelocity, edge->maxVelocity);
      const double segmentRemaining = std::max(0.0, segmentLength - car.distanceAlongSegment);

      if (segmentRemaining <= 0.0)
      {
        car.currentPosition = destinationPosition;
        car.distanceAlongSegment = 0.0;
        ++car.currentRouteSegment;
        continue;
      }

      const double timeToNextNode = segmentRemaining / car.velocity;
      if (remainingTime >= timeToNextNode)
      {
        // Finish this segment, then continue onto the next route edge if time remains.
        remainingTime -= timeToNextNode;
        car.currentPosition = destinationPosition;
        car.distanceAlongSegment = 0.0;
        ++car.currentRouteSegment;
        if (car.currentRouteSegment + 1 == car.routeNodeIndices.size())
          break;
      }
      else
      {
        // Interpolate the car's geographic position within its current segment.
        car.distanceAlongSegment += car.velocity * remainingTime;
        car.currentPosition = GeoUtils::positionAlongSegment(sourcePosition, destinationPosition,
                                                             car.distanceAlongSegment);
        remainingTime = 0.0;
        break;
      }
    }

    // Mark completed routes and stop the car at its destination.
    if (car.currentRouteSegment + 1 == car.routeNodeIndices.size())
    {
      car.arrived = true;
      car.currentPosition = car.endPosition;
      car.velocity = 0.0;
    }
  }
  return true;
}

void Traffic::clear()
{
  // Remove existing cars when a new simulation network is loaded.
  m_cars.clear();
}

const std::vector<TrafficCar>& Traffic::cars() const { return m_cars; }
