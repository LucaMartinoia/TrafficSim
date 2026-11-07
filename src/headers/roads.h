#pragma once

#include <vector>

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QString>
#include <QVariantList>

// RoadEdge and RoadNode create the adjacency list for the graph representation
struct RoadEdge
{
  std::size_t destination;
  double length;
  double maxVelocity;
  double minTime;
  double travelTime;
  int lanes;
};

struct RoadNode
{
  QGeoCoordinate coordinate;
  std::vector<RoadEdge> edges;

  // Constructor
  RoadNode(const QGeoCoordinate& coordinate) : coordinate(coordinate) {}
};

// Identifies a node-edge pair in the adjacency list
struct RoadEdgeId
{
  std::size_t sourceNode;
  std::size_t edgeIndex;
};

// "using" creates Aliases for the vectors
using RoadRoute = std::vector<RoadEdgeId>;
using EdgeOccupancy = std::vector<std::vector<std::size_t>>;

enum class WayDirection
{
  Forward,
  Reverse,
  Bidirectional
};

struct ParsedWay
{
  std::vector<QGeoCoordinate> points;
  WayDirection direction;
  double maxVelocity;
  int lanes;
};

// Enum to list the weight to use in computing paths
enum class RoutingMetric
{
  Distance,
  MinTime,
  TravelTime
};

// Enum to list the possible algorithms to use in computing paths
enum class Algorithm
{
  Dijkstra,
  Astar
};

class RoadNetwork
{
public:
  // Build the adjacency list from OSM data
  bool buildRoadNetwork(const QJsonDocument& response);
  // Clear graph data while retaining the selected routing metric
  void clear();
  // Find a directed route as edge references using the selected edge cost
  bool findRoute(std::size_t startNode, std::size_t endNode, RoadRoute& route) const;
  // Refresh only the adaptive travel-time weights from the current occupancy snapshot
  void resetOccupancy();
  void updateTravelTimes();
  // Convert parsed ways into paths suitable for display
  QVariantList roadsPolyline() const;

  // Getter methods
  const std::vector<RoadNode>& nodes() const;
  const std::vector<ParsedWay>& ways() const;

  // Setter methods
  void setRoutingMetric(RoutingMetric metric);
  void setRoutingMetric(QString metric);
  void incrementOccupancy(const RoadEdgeId id);

private:
  bool parseOSMResponse(const QJsonDocument& response);
  bool Dijkstra(std::size_t startNode, std::size_t endNode, RoadRoute& route) const;
  bool Astar(std::size_t startNode, std::size_t endNode, RoadRoute& route, double weight) const;
  double heuristic(std::size_t node, std::size_t end) const;

  std::vector<ParsedWay> m_parsedWays;
  std::vector<RoadNode> m_nodes;
  Algorithm m_algorithm = Algorithm::Dijkstra;
  RoutingMetric m_routingMetric = RoutingMetric::Distance;
  EdgeOccupancy m_occupancy;
};
