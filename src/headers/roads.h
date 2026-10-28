#pragma once

#include <cstddef>
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
  double travelTime;
};

struct RoadNode
{
  QGeoCoordinate coordinate;
  std::vector<RoadEdge> edges;

  RoadNode(const QGeoCoordinate& coordinate) : coordinate(coordinate) {}
};

// Enum to list the weight to use in computing paths
enum class RoutingMetric
{
  Distance,
  TravelTime,
  AdaptiveTime
};

class RoadNetwork
{
public:
  // Build the adjacency list from OSM data
  bool buildRoadNetwork(const QJsonDocument& response, QString& error);
  // Convert OSM data into paths suitable for display
  static QVariantList roadsPolyline(const QJsonDocument& response);

  // Getter methods
  const std::vector<RoadNode>& nodes() const;

private:
  std::vector<RoadNode> m_nodes;
  std::vector<RoadNode> m_reduced_nodes;
};
