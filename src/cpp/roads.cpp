#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <utility>

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

// Returns time in seconds
double travelTime(double length, double velocity) { return length / velocity; }

// Add an edge between coordinates already validated by parseOSMResponse
void addEdge(RoadNode& current, const QGeoCoordinate& neighborCoordinate,
             const std::map<QGeoCoordinate, std::size_t, comparator>& nodeIndices, RoadEdge edge)
{
  // Fill the geometry-dependent fields and append this outgoing edge
  edge.destination = nodeIndices.find(neighborCoordinate)->second;
  edge.length = GeoUtils::distance(current.coordinate, neighborCoordinate);
  edge.minTime = travelTime(edge.length, edge.maxVelocity);
  edge.travelTime = edge.minTime;
  current.edges.push_back(edge);
}

/*
TAG PARSING
*/

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

// Parse the OSM tags to read the velocity
double maxVelocity(const QJsonObject& tags)
{
  QString speedTag = tags.value(QStringLiteral("maxspeed")).toString().trimmed().toLower();
  speedTag = speedTag.section(QLatin1Char(';'), 0, 0).trimmed();
  static const QRegularExpression speedPattern(
      QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)\\s*(mph|km/?h|kmh|kph)?$"));
  const QRegularExpressionMatch match = speedPattern.match(speedTag);
  // If no velocity, default to 50 km/h
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

int laneCount(const QJsonObject& tags)
{
  // Treat absent or unusable lane tags as a single lane
  bool isNumeric = false;
  const int lanes = tags.value(QStringLiteral("lanes")).toString().toInt(&isNumeric);
  return isNumeric && lanes > 0 ? lanes : 1;
}
} // namespace

/*
BUILDING NETWORK
*/

// Build the adjacency list for the road graph
bool RoadNetwork::buildRoadNetwork(const QJsonDocument& response)
{
  // Parse and validate the JSON before replacing the current graph
  if (!parseOSMResponse(response))
    return false;

  // Build graph nodes from the validated intermediate representation
  std::vector<RoadNode> parsedNodes;
  std::map<QGeoCoordinate, std::size_t, comparator> nodeIndices;
  for (const ParsedWay& way : m_parsedWays)
  {
    for (const QGeoCoordinate& coordinate : way.points)
    {
      if (!nodeIndices.contains(coordinate))
      {
        const std::size_t nodeIndex = parsedNodes.size();
        nodeIndices.emplace(coordinate, nodeIndex);
        parsedNodes.emplace_back(coordinate);
      }
    }
  }

  std::size_t edgeCount = 0;
  // Create directed edges from each adjacent pair in a parsed way
  for (const ParsedWay& way : m_parsedWays)
  {
    RoadEdge edge{};
    edge.maxVelocity = way.maxVelocity;
    edge.lanes = way.lanes;
    for (std::size_t pointIndex = 0; pointIndex + 1 < way.points.size(); ++pointIndex)
    {
      const QGeoCoordinate& currentCoordinate = way.points[pointIndex];
      const QGeoCoordinate& nextCoordinate = way.points[pointIndex + 1];
      if (way.direction != WayDirection::Forward)
      {
        const std::size_t currentNode = nodeIndices.find(nextCoordinate)->second;
        RoadNode& current = parsedNodes[currentNode];
        addEdge(current, currentCoordinate, nodeIndices, edge);
        ++edgeCount;
      }
      if (way.direction != WayDirection::Reverse)
      {
        const std::size_t currentNode = nodeIndices.find(currentCoordinate)->second;
        RoadNode& current = parsedNodes[currentNode];
        addEdge(current, nextCoordinate, nodeIndices, edge);
        ++edgeCount;
      }
    }
  }

  // Replace the graph only after the complete response has parsed successfully
  m_nodes.swap(parsedNodes);
  qInfo() << "Parsed" << m_parsedWays.size() << "OSM way segments into" << m_nodes.size()
          << "nodes and" << edgeCount << "directed edges.";

  // Initialize the occupancy adjacenct list counters
  m_occupancy.clear();
  m_occupancy.reserve(m_nodes.size());

  for (const RoadNode& node : m_nodes)
    m_occupancy.emplace_back(node.edges.size(), 0);

  return true;
}

// Convert valid OSM way data into the intermediate representation
bool RoadNetwork::parseOSMResponse(const QJsonDocument& response)
{
  // Validate the response envelope before examining individual elements
  if (!response.isObject() || !response.object().value(QStringLiteral("elements")).isArray())
  {
    qWarning() << "Overpass API response must contain an elements array";
    return false;
  }

  const QJsonArray elements = response.object().value(QStringLiteral("elements")).toArray();
  std::vector<ParsedWay> parsedWays;
  for (const QJsonValue& elementValue : elements)
  {
    // Skip malformed elements and non-way elements
    if (!elementValue.isObject())
      continue;

    const QJsonObject element = elementValue.toObject();
    if (element.value(QStringLiteral("type")).toString() != QStringLiteral("way"))
      continue;

    const QJsonValue geometryValue = element.value(QStringLiteral("geometry"));
    if (!geometryValue.isArray())
      continue;

    // Gather tags data
    const QJsonObject tags = element.value(QStringLiteral("tags")).toObject();
    ParsedWay way{};
    way.direction = wayDirection(tags);
    way.maxVelocity = maxVelocity(tags);
    way.lanes = laneCount(tags);

    // Keep valid coordinates and skip malformed points
    for (const QJsonValue& pointValue : geometryValue.toArray())
    {
      if (!pointValue.isObject())
        continue;

      const QJsonObject point = pointValue.toObject();
      const QJsonValue latitude = point.value(QStringLiteral("lat"));
      const QJsonValue longitude = point.value(QStringLiteral("lon"));
      const QGeoCoordinate coordinate(latitude.toDouble(), longitude.toDouble());
      if (!coordinate.isValid())
        continue;

      // Ignore adjacent duplicates to avoid zero-length self-edges
      if (way.points.empty() || way.points.back() != coordinate)
        way.points.push_back(coordinate);
    }

    // Retain only ways with at least one usable segment
    if (way.points.size() >= 2)
      parsedWays.push_back(std::move(way));
  }

  if (parsedWays.empty())
  {
    qWarning() << "Road network must contain at least one valid edge";
    return false;
  }

  // Replace the intermediate representation only after parsing a usable network
  m_parsedWays.swap(parsedWays);
  return true;
}

// Produce a polyline for display
QVariantList RoadNetwork::roadsPolyline() const
{
  // Convert the validated way segments into paths for MapPolyline
  QVariantList paths;
  for (const ParsedWay& way : m_parsedWays)
  {
    QVariantList path;
    for (const QGeoCoordinate& point : way.points)
      path.append(QVariant::fromValue(point));
    paths.append(QVariant::fromValue(path));
  }
  return paths;
}

/*
ROUTE FINDING
*/

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
  // Min-priority queue ordered by cost (std::greater reverses the default max-heap ordering)
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
      else if (m_routingMetric == RoutingMetric::MinTime)
        edgeCost = edge.minTime;
      else if (m_routingMetric == RoutingMetric::TravelTime)
        edgeCost = edge.travelTime;

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

/*
OCCUPANCY LIST METHODS
*/

// Reset to zero the occupancy counters
void RoadNetwork::resetOccupancy()
{
  if (m_routingMetric != RoutingMetric::TravelTime)
    return;

  for (auto& nodeOccupancy : m_occupancy)
    std::fill(nodeOccupancy.begin(), nodeOccupancy.end(), 0);
}

// Increment the occupancy counter for an edge in the adaptive routing metric
void RoadNetwork::incrementOccupancy(const RoadEdgeId id)
{
  if (m_routingMetric != RoutingMetric::TravelTime)
    return;

  // Ignore edge references that do not match the current graph
  if (id.sourceNode >= m_occupancy.size() || id.edgeIndex >= m_occupancy[id.sourceNode].size())
    return;

  ++m_occupancy[id.sourceNode][id.edgeIndex];
}

// Update the travel times according to the occupancy
void RoadNetwork::updateTravelTimes()
{
  if (m_routingMetric != RoutingMetric::TravelTime)
    return;

  for (std::size_t nodeIndex = 0; nodeIndex < m_nodes.size(); ++nodeIndex)
  {
    RoadNode& node = m_nodes[nodeIndex];
    for (std::size_t edgeIndex = 0; edgeIndex < node.edges.size(); ++edgeIndex)
    {
      RoadEdge& edge = node.edges[edgeIndex];
      const std::size_t cars = m_occupancy[nodeIndex][edgeIndex];
      edge.travelTime = edge.minTime;

      // Delay scales linearly with cars
      if (cars > 1)
        edge.travelTime *= 1.0 + static_cast<double>(cars - 1) / static_cast<double>(edge.lanes);
    }
  }
}

// Clear graph data
void RoadNetwork::clear()
{
  m_parsedWays.clear();
  m_nodes.clear();
  m_occupancy.clear();
}

/*
GETTER METHODS
*/

// Getter methods
const std::vector<RoadNode>& RoadNetwork::nodes() const { return m_nodes; }
const std::vector<ParsedWay>& RoadNetwork::ways() const { return m_parsedWays; }

/*
SETTER METHODS
*/

// Setter methods
void RoadNetwork::setRoutingMetric(QString metric)
{
  if (metric == QStringLiteral("Distance"))
    setRoutingMetric(RoutingMetric::Distance);
  else if (metric == QStringLiteral("Travel time")) // TODO: fix
    setRoutingMetric(RoutingMetric::MinTime);
  else if (metric == QStringLiteral("Adaptive time"))
    setRoutingMetric(RoutingMetric::TravelTime);
  else
  {
    qWarning() << "Ignoring unknown routing metric:" << metric;
  }
}

// Keep enum-based callers available alongside the UI-facing string setter
void RoadNetwork::setRoutingMetric(RoutingMetric metric) { m_routingMetric = metric; }
