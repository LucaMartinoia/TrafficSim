#include <cmath>
#include <map>
#include <set>

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
// Enum to store the direction of a given way
enum class WayDirection
{
  Forward,
  Reverse,
  Bidirectional
};

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

double maxVelocity(const QJsonObject& tags)
{
  // Parse the first usable OSM maxspeed value, treating unitless values as km/h
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

bool addEdgeToNeighbor(RoadNode& current, std::size_t currentNode,
                       const QGeoCoordinate& currentCoordinate, const QJsonArray& geometry,
                       int neighborPointIndex,
                       const std::map<QGeoCoordinate, std::size_t, comparator>& nodeIndices,
                       double velocity)
{
  // Resolve the neighbor and skip geometry points that map to the current node.
  const QJsonObject neighborPoint = geometry.at(neighborPointIndex).toObject();
  const QGeoCoordinate neighborCoordinate(neighborPoint.value(QStringLiteral("lat")).toDouble(),
                                          neighborPoint.value(QStringLiteral("lon")).toDouble());
  const std::size_t neighborNode = nodeIndices.find(neighborCoordinate)->second;
  if (neighborNode == currentNode)
    return false;

  // Calculate the edge weights and append the outgoing edge.
  const double length = GeoUtils::distance(currentCoordinate, neighborCoordinate);
  const double time = travelTime(length, velocity);
  current.edges.push_back({neighborNode, length, velocity, time});
  return true;
}

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
        if (addEdgeToNeighbor(current, currentNode, currentCoordinate, geometry, pointIndex - 1,
                              nodeIndices, velocity))
          ++edgeCount;
      }
      if (direction != WayDirection::Reverse && pointIndex + 1 < geometry.size())
      {
        if (addEdgeToNeighbor(current, currentNode, currentCoordinate, geometry, pointIndex + 1,
                              nodeIndices, velocity))
          ++edgeCount;
      }
    }
  }

  // Replace the graph only after the complete response has parsed successfully
  // TODO: remove edgeCount and wayCounts
  m_nodes.swap(parsedNodes);
  m_reduced_nodes.clear();
  qInfo() << "Parsed" << wayCount << "ways into" << m_nodes.size() << "nodes and" << edgeCount
          << "directed edges.";
  return true;
}

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

  return paths;
}

// Getter methods
const std::vector<RoadNode>& RoadNetwork::nodes() const { return m_nodes; }