#include <algorithm>
#include <cmath>
#include <functional> // TODO: remove this crap
#include <limits>
#include <map>
#include <queue>

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>

#include "geo_utils.h"
#include "roads.h"

// Nameless namespace to keep helper scoped to this module only
namespace
{
// Comparator necessary to use std::map with QGeoCoordinate as keys
struct comparator
{
  bool operator()(const QGeoCoordinate& left, const QGeoCoordinate& right) const
  {
    if (left.latitude() != right.latitude())
      return left.latitude() < right.latitude();
    return left.longitude() < right.longitude();
  }
};

// Parse the OSM tags to read the velocity
double maxVelocity(const QJsonObject& tags)
{
  // TODO: consider improving the tag parser
  QString speedTag = tags.value(QStringLiteral("maxspeed")).toString().trimmed().toLower();
  speedTag = speedTag.section(QLatin1Char(';'), 0, 0).trimmed();
  static const QRegularExpression speedPattern(
      QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*(mph|km/?h|kmh|kph)?$"));
  const QRegularExpressionMatch match = speedPattern.match(speedTag);
  if (!match.hasMatch())
    return 50.0 / 3.6;

  bool isNumeric = false;
  const double speed = match.captured(1).toDouble(&isNumeric);
  if (!isNumeric || !std::isfinite(speed) || speed <= 0.0)
    return 50.0 / 3.6;

  // Convert supported OSM speed units to the m/s
  if (match.captured(2) == QStringLiteral("mph"))
    return speed * 0.44704;
  return speed / 3.6;
}

// Returns time in seconds
double travelTime(double length, double velocity) { return length / velocity; }

// Add edges to the adjacency map
bool addEdge(RoadNode& current, const QJsonArray& geometry, int neighborPointIndex,
             const std::map<QGeoCoordinate, std::size_t, comparator>& nodeIndices, double velocity)
{
  // Resolve the neighbor and skip geometry points that map to the current node.
  const QJsonObject neighborPoint = geometry.at(neighborPointIndex).toObject();
  const QGeoCoordinate neighborCoordinate(neighborPoint.value(QStringLiteral("lat")).toDouble(),
                                          neighborPoint.value(QStringLiteral("lon")).toDouble());
  const std::size_t neighborNode = nodeIndices.find(neighborCoordinate)->second;
  if (neighborCoordinate == current.coordinate)
    return false;

  // Calculate the edge weights and append the outgoing edge.
  const double length = GeoUtils::distance(current.coordinate, neighborCoordinate);
  const double time = travelTime(length, velocity);
  current.edges.push_back({neighborNode, length, velocity, time, time});
  return true;
}

// Check validity of the OSM data
bool validateOSMResponse(const QJsonDocument& response, QString& error)
{
  // Clear a previous error before validating the response envelope.
  error.clear();

  if (!response.isObject() || !response.object().value(QStringLiteral("elements")).isArray())
  {
    error = QStringLiteral("Overpass response must contain an elements array.");
    return false;
  }

  return true;
}

// Enum to store the direction of a given way
enum class WayDirection
{
  Forward,
  Reverse,
  Bidirectional
};

// Retrive the directionality of a given road
WayDirection wayDirection(const QJsonObject& tags)
{
  // Interpret explicit one-way tags and the OSM-implied one-way road types
  const QString oneWay = tags.value(QStringLiteral("oneway")).toString().trimmed().toLower();
  const QString junction = tags.value(QStringLiteral("junction")).toString().toLower();
  const QString highway = tags.value(QStringLiteral("highway")).toString().toLower();
  if (oneWay == QStringLiteral("-1"))
    return WayDirection::Reverse;

  if (oneWay == QStringLiteral("yes") || oneWay == QStringLiteral("true") ||
      oneWay == QStringLiteral("1") ||
      (oneWay.isEmpty() &&
       (junction == QStringLiteral("roundabout") || junction == QStringLiteral("circular") ||
        highway == QStringLiteral("motorway") || highway == QStringLiteral("motorway_link"))))
    return WayDirection::Forward;

  return WayDirection::Bidirectional;
}
} // namespace

// Build the adjacency list for the road graph
bool RoadNetwork::buildRoadNetwork(const QJsonDocument& response, QString& error)
{
  // Validate the response before building a replacement graph
  if (!validateOSMResponse(response, error))
    return false;

  // We use parsedNodes instead of the class member to avoid mid-parsing errors
  std::vector<RoadNode> parsedNodes;
  // Dictionary to get node index from its coordinates
  std::map<QGeoCoordinate, std::size_t, comparator> nodeIndices;

  const QJsonArray elements = response.object().value(QStringLiteral("elements")).toArray();
  qInfo() << "Parsing road network from" << elements.size() << "OSM elements.";
  std::size_t wayCount = 0;
  std::size_t edgeCount = 0;

  // First pass: validate way geometry and populate parsedNodes with unique nodes
  for (const QJsonValue& elementValue : elements)
  {
    // Check if object
    if (!elementValue.isObject())
    {
      error = QStringLiteral("OSM element is the returned JSON is not an object.");
      return false;
    }

    // Check if way type
    const QJsonObject element = elementValue.toObject();
    if (element.value(QStringLiteral("type")).toString() != QStringLiteral("way"))
      continue;

    // Check if valid geometry
    const QJsonValue geometryValue = element.value(QStringLiteral("geometry"));
    if (!geometryValue.isArray() || geometryValue.toArray().size() < 2)
    {
      error = QStringLiteral("OSM way %1 has no usable geometry.")
                  .arg(element.value(QStringLiteral("id")).toVariant().toString());
      return false;
    }

    // If valid entry
    ++wayCount;
    const QJsonArray geometry = geometryValue.toArray();
    for (const QJsonValue& pointValue : geometry)
    {
      // Check if object
      if (!pointValue.isObject())
      {
        error = QStringLiteral("OSM way geometry contains a non-object point.");
        return false;
      }

      const QJsonObject point = pointValue.toObject();
      const QJsonValue latitudeValue = point.value(QStringLiteral("lat"));
      const QJsonValue longitudeValue = point.value(QStringLiteral("lon"));
      // Check if coordinates are valid
      if (!latitudeValue.isDouble() || !longitudeValue.isDouble())
      {
        error = QStringLiteral("OSM way geometry point is missing numeric coordinates.");
        return false;
      }
      const QGeoCoordinate coordinate(latitudeValue.toDouble(), longitudeValue.toDouble());
      if (!coordinate.isValid())
      {
        error = QStringLiteral("OSM way geometry contains an invalid coordinate.");
        return false;
      }

      // Register coordinates once so ways sharing a point share a graph node.
      if (!nodeIndices.contains(coordinate))
      {
        const std::size_t nodeIndex = parsedNodes.size();
        // Add new node to the map
        nodeIndices.emplace(coordinate, nodeIndex);
        // And to the adjacency list
        RoadNode newNode(coordinate);
        parsedNodes.push_back(newNode);
      }
    }
  }

  // Not a network
  if (parsedNodes.size() < 2)
  {
    error = QStringLiteral("Road network must contain at least two nodes.");
    return false;
  }

  // Second pass: add each way's outgoing edges to the current node
  for (const QJsonValue& elementValue : elements)
  {
    const QJsonObject element = elementValue.toObject();

    // Check if way type
    if (element.value(QStringLiteral("type")).toString() != QStringLiteral("way"))
      continue;

    // Gather way's geometry, direction and velocity data
    const QJsonArray geometry = element.value(QStringLiteral("geometry")).toArray();
    const QJsonObject tags = element.value(QStringLiteral("tags")).toObject();
    const WayDirection direction = wayDirection(tags);
    const double velocity = maxVelocity(tags);

    // Loop over all points in the given way
    for (int pointIndex = 0; pointIndex < geometry.size(); ++pointIndex)
    {
      // Retrive current node data from the nodeIndices dictionary
      const QJsonObject currentPoint = geometry.at(pointIndex).toObject();
      const QGeoCoordinate currentCoordinate(currentPoint.value(QStringLiteral("lat")).toDouble(),
                                             currentPoint.value(QStringLiteral("lon")).toDouble());
      const std::size_t currentNode = nodeIndices.find(currentCoordinate)->second;
      RoadNode& current = parsedNodes[currentNode];

      // Select adjacent points according to the way's travel direction
      if (direction != WayDirection::Forward && pointIndex > 0)
      {
        if (addEdge(current, geometry, pointIndex - 1, nodeIndices, velocity))
          ++edgeCount;
      }
      if (direction != WayDirection::Reverse && pointIndex + 1 < geometry.size())
      {
        if (addEdge(current, geometry, pointIndex + 1, nodeIndices, velocity))
          ++edgeCount;
      }
    }
  }

  // Reject the network if every candidate edge was invalid.
  if (edgeCount == 0)
  {
    error = QStringLiteral("Road network must contain at least one valid edge.");
    return false;
  }

  // Replace the graph only after the complete response has parsed successfully
  // TODO: remove edgeCount and wayCounts
  m_nodes.swap(parsedNodes);
  qInfo() << "Parsed" << wayCount << "ways into" << m_nodes.size() << "nodes and" << edgeCount
          << "directed edges.";

  // Initialize the occupancy adjacenct list counters
  m_occupancy.reserve(m_nodes.size());

  for (const RoadNode& node : m_nodes)
    m_occupancy.emplace_back(node.edges.size(), 0);

  return true;
}

// Produce a polyline for display
QVariantList RoadNetwork::roadsPolyline(const QJsonDocument& response)
{
  // Convert each way's valid geometry points into a path for MapPolyline
  const QJsonArray elements = response.object().value(QStringLiteral("elements")).toArray();
  QVariantList paths;
  for (const QJsonValue& elementValue : elements)
  {
    // Skip non-way types
    const QJsonObject element = elementValue.toObject();
    if (element.value(QStringLiteral("type")).toString() != QStringLiteral("way"))
      continue;

    QVariantList path;
    const QJsonArray geometry = element.value(QStringLiteral("geometry")).toArray();
    for (const QJsonValue& pointValue : geometry)
    {
      const QJsonObject point = pointValue.toObject();
      const QGeoCoordinate coordinate(point.value(QStringLiteral("lat")).toDouble(),
                                      point.value(QStringLiteral("lon")).toDouble());
      if (coordinate.isValid())
        path.append(QVariant::fromValue(coordinate));
    }

    if (path.size() >= 2)
      paths.append(QVariant::fromValue(path));
  }

  // Logging purposes
  int geometryPoints = 0;
  for (const QVariant& path : paths)
    geometryPoints += path.toList().size();

  // Dispatch prepared paths to the map and report the processed response
  qInfo() << "Overpass request succeeded:" << elements.size() << "elements," << paths.size()
          << "ways," << geometryPoints << "geometry points.";

  return paths;
}

// Compute the route between two nodes
bool RoadNetwork::findRoute(std::size_t startNode, std::size_t endNode, RoadRoute& route) const
{
  // Basic validity checks
  if (startNode >= m_nodes.size() || endNode >= m_nodes.size() || startNode == endNode)
    return false;

  route.clear();

  // Switch case for the path finding algorithm
  if (m_algorithm == Algorithm::Dijkstra)
  {
    return Dijkstra(startNode, endNode, route);
  }
  else if (m_algorithm == Algorithm::Astar)
  {
    return Astar(startNode, endNode, route, 1);
  }
  else
    return false;
}

// Dijkstra implementation
bool RoadNetwork::Dijkstra(std::size_t startNode, std::size_t endNode, RoadRoute& route) const
{
  return Astar(startNode, endNode, route, 0);
}

// A* implementation
bool RoadNetwork::Astar(std::size_t startNode, std::size_t endNode, RoadRoute& route,
                        double weight) const
{
  // Track the best known cost and predecessor edge for each graph node
  const double infinity = std::numeric_limits<double>::infinity();
  // The edge cost
  std::vector<double> costs(m_nodes.size(), infinity);
  // The edge to the previous node
  std::vector<RoadEdgeId> predecessors(m_nodes.size());
  // Min-priority queue ordered by cost (std::greater reverses the default max-heap ordering).
  using QueueEntry = std::tuple<double, double, std::size_t>;
  std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
  // Initialize starting node
  costs[startNode] = 0.0;
  double priority = weight * heuristic(startNode, endNode);
  queue.emplace(priority, 0.0, startNode);

  // Visit the lowest-cost reachable node until the destination is finalized
  while (!queue.empty())
  {
    // Select current node
    const auto [_, currentCost, currentNode] = queue.top();
    queue.pop();
    // Prune old queue entries
    if (currentCost > costs[currentNode])
      continue;
    if (currentNode == endNode)
      break;

    // Loop over outgoing edges
    const std::vector<RoadEdge>& edges = m_nodes[currentNode].edges;
    for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
    {
      const RoadEdge& edge = edges[edgeIndex];
      // Find edge cost depending on selected metric
      double edgeCost = -1.;
      if (m_routingMetric == RoutingMetric::Distance)
        edgeCost = edge.length;
      else if (m_routingMetric == RoutingMetric::TravelTime)
        edgeCost = edge.freeFlowTime;
      else if (m_routingMetric == RoutingMetric::AdaptiveTime)
        edgeCost = edge.adaptiveTime;

      if (!std::isfinite(edgeCost) || edgeCost < 0.0)
        continue;

      // If the new cost is smaller, update the cost and the queue
      const double candidateCost = currentCost + edgeCost;
      if (candidateCost >= costs[edge.destination])
        continue;

      costs[edge.destination] = candidateCost;
      predecessors[edge.destination] = {currentNode, edgeIndex};
      priority = candidateCost + weight * heuristic(edge.destination, endNode);
      queue.emplace(priority, candidateCost, edge.destination);
    }
  }

  // If destination cannot be reached, return false
  if (!std::isfinite(costs[endNode]))
    return false;

  // Reconstruct edge references from the destination back to the start
  for (std::size_t node = endNode; node != startNode;)
  {
    const RoadEdgeId& edgeId = predecessors[node];

    route.push_back(edgeId);
    node = edgeId.sourceNode;
  }

  std::reverse(route.begin(), route.end());
  return true;
}

// Function to compute the heuristic
double RoadNetwork::heuristic(std::size_t node, std::size_t end) const
{
  const double distance = GeoUtils::distance(m_nodes[node].coordinate, m_nodes[end].coordinate);

  if (m_routingMetric == RoutingMetric::Distance)
    return distance;

  constexpr double maxVelocity = 130.0 / 3.6; // m/s
  return distance / maxVelocity;
}

// Reset to zero the occupancy counters
void RoadNetwork::resetOccupancy()
{
  if (m_routingMetric != RoutingMetric::AdaptiveTime)
    return;

  for (auto& nodeOccupancy : m_occupancy)
    std::fill(nodeOccupancy.begin(), nodeOccupancy.end(), 0);
}

// Increment the occupancy counter for an edge in the adaptive routing metric
void RoadNetwork::incrementOccupancy(const RoadEdgeId id)
{
  if (m_routingMetric != RoutingMetric::AdaptiveTime)
    return;

  // Ignore edge references that do not match the current graph
  if (id.sourceNode >= m_occupancy.size() || id.edgeIndex >= m_occupancy[id.sourceNode].size())
    return;

  ++m_occupancy[id.sourceNode][id.edgeIndex];
}

// Update the travel times according to the occupancy
void RoadNetwork::updateTravelTimes()
{
  if (m_routingMetric != RoutingMetric::AdaptiveTime)
    return;

  for (RoadNode& node : m_nodes)
  {
    for (RoadEdge& edge : node.edges)
    {
      // TODO: set formula
      edge.adaptiveTime = edge.freeFlowTime;
    }
  }
}

// Getter methods
const std::vector<RoadNode>& RoadNetwork::nodes() const { return m_nodes; }

// Setter methods
void RoadNetwork::setRoutingMetric(RoutingMetric metric) { m_routingMetric = metric; }
