#include <cmath>
#include <vector>

#include <QJsonDocument>
#include <QJsonObject>
#include <QGeoCoordinate>
#include <QSignalSpy>
#include <QtTest>

#include "OSMclient.h"
#include "geo_utils.h"
#include "roads.h"
#include "simulation.h"
#include "traffic.h"

class RoadNetworkTests : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    // Give the Overpass integration test a stable application cache namespace.
    QCoreApplication::setOrganizationName(QStringLiteral("TrafficSimTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TrafficSimTests"));
  }

  void roadNetworkBuildsAdjacencyList()
  {
    // Define a straight road with a pass-through point and a one-way branch at its junction.
    const QByteArray json = R"json(
      {
        "elements": [
          {
            "type": "way",
            "id": 1,
            "tags": {"highway": "residential", "maxspeed": "36"},
            "geometry": [
              {"lat": 44.0, "lon": 8.0},
              {"lat": 44.0, "lon": 8.001},
              {"lat": 44.0, "lon": 8.002},
              {"lat": 44.0, "lon": 8.003}
            ]
          },
          {
            "type": "way",
            "id": 2,
            "tags": {"highway": "residential", "oneway": "yes", "maxspeed": "30 mph"},
            "geometry": [
              {"lat": 44.0, "lon": 8.002},
              {"lat": 44.001, "lon": 8.002}
            ]
          }
        ]
      }
    )json";

    // Parse the fixture and verify that shared coordinates become shared graph nodes.
    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error), qPrintable(error));
    QCOMPARE(network.nodes().size(), std::size_t(5));

    // Verify full-graph edge direction, speed metadata, and computed travel time.
    const std::vector<RoadNode>& nodes = network.nodes();
    QVERIFY(nodes[0].edges.size() == 1);
    QCOMPARE(nodes[0].edges[0].destination, std::size_t(1));
    QCOMPARE(nodes[0].edges[0].maxVelocity, 10.0);
    QVERIFY(nodes[0].edges[0].freeFlowTime > 0.0);
    QVERIFY(std::abs(nodes[0].edges[0].freeFlowTime - nodes[0].edges[0].length / 10.0) < 0.001);
    QCOMPARE(nodes[0].edges[0].adaptiveTime, nodes[0].edges[0].freeFlowTime);
    QVERIFY(nodes[1].edges.size() == 2);
    QCOMPARE(nodes[1].edges[1].destination, std::size_t(2));
    QCOMPARE(nodes[2].edges[2].destination, std::size_t(4));
    QVERIFY(std::abs(nodes[2].edges[2].maxVelocity - 30.0 * 0.44704) < 0.000001);
    QVERIFY(nodes[4].edges.empty());

    // Reject distinct nodes with only invalid self-edges and preserve the current graph.
    const QByteArray disconnectedNetworkJson = R"json(
      {
        "elements": [
          {
            "type": "way",
            "geometry": [
              {"lat": 44.0, "lon": 8.0},
              {"lat": 44.0, "lon": 8.0}
            ]
          },
          {
            "type": "way",
            "geometry": [
              {"lat": 44.001, "lon": 8.0},
              {"lat": 44.001, "lon": 8.0}
            ]
          }
        ]
      }
    )json";
    QVERIFY(!network.buildRoadNetwork(QJsonDocument::fromJson(disconnectedNetworkJson), error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(network.nodes().size(), std::size_t(5));
  }

  void boundingBoxRequiresValidOrderedCoordinates()
  {
    // Accept top-left and bottom-right coordinates in geographic order
    const QGeoCoordinate topLeft(45.0, 8.0);
    const QGeoCoordinate bottomRight(44.0, 9.0);
    QVERIFY(GeoUtils::isValidBBox(topLeft, bottomRight));

    // Reject invalid coordinates and corners with reversed ordering
    QVERIFY(!GeoUtils::isValidBBox(QGeoCoordinate(91.0, 8.0), bottomRight));
    QVERIFY(!GeoUtils::isValidBBox(bottomRight, topLeft));
  }

  void mapViewRequiresValidCoordinateAndZoom()
  {
    // Accept valid geographic coordinates with an in-range zoom
    QVERIFY(GeoUtils::isValidMapView(QGeoCoordinate(44.0, 8.0), 18));

    // Reject invalid geographic coordinates and zoom levels outside the supported range
    QVERIFY(!GeoUtils::isValidMapView(QGeoCoordinate(), 18));
    QVERIFY(!GeoUtils::isValidMapView(QGeoCoordinate(44.0, 8.0), 23));
  }

  void defaultOverpassQueryBuildsRoadNetworks()
  {
    // Observe both outcomes from the real default query, including its cache path.
    OSMClient client;
    QSignalSpy succeededSpy(&client, &OSMClient::requestSucceeded);
    QVERIFY(succeededSpy.isValid());

    // Start the live request and wait for a valid result from any public instance.
    client.requestDefault();
    QTRY_VERIFY_WITH_TIMEOUT(succeededSpy.count() > 0, 65000);

    // Validate that the downloaded JSON produces a non-empty graph with road edges.
    const QJsonDocument response = succeededSpy.takeFirst().at(1).value<QJsonDocument>();
    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(response, error), qPrintable(error));
    QVERIFY(!network.nodes().empty());

    std::size_t edgeCount = 0;
    for (const RoadNode& node : network.nodes())
      edgeCount += node.edges.size();
    QVERIFY(edgeCount > 0);
  }

  void simulationDefersUntilTrafficUpdateIsImplemented()
  {
    // Start with one directed road and verify the simulation time-step lifecycle.
    const QByteArray json = R"json(
      {
        "elements": [
          {
            "type": "way",
            "tags": {"highway": "residential", "oneway": "yes"},
            "geometry": [
              {"lat": 44.0, "lon": 8.0},
              {"lat": 44.0, "lon": 8.001}
            ]
          }
        ]
      }
    )json";

    Simulation simulation;
    QString error;
    QVERIFY(!simulation.update(error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(simulation.time(), 0.0);

    QVERIFY2(simulation.init(QJsonDocument::fromJson(json), error), qPrintable(error));
    QCOMPARE(simulation.time(), 0.0);
    QVERIFY(simulation.timeStep() > 0.0);

    // The simulation reports that traffic movement is not implemented yet.
    QVERIFY(!simulation.update(error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(simulation.time(), 0.0);

    simulation.clear();
    QVERIFY(!simulation.update(error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(simulation.time(), 0.0);
  }

  void trafficSkipsCarWhenRouteIsUnavailable()
  {
    // Build one segment with a 36 km/h limit, equivalent to 10 meters per second.
    const QByteArray json = R"json(
      {
        "elements": [
          {
            "type": "way",
            "tags": {"highway": "residential", "oneway": "yes", "maxspeed": "36"},
            "geometry": [
              {"lat": 44.0, "lon": 8.0},
              {"lat": 44.0, "lon": 8.001}
            ]
          }
        ]
      }
    )json";

    // Parse the road and try to create a car while route finding is still a stub.
    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error), qPrintable(error));
    Traffic traffic;
    error = QStringLiteral("previous error");
    QVERIFY(!traffic.addCar(network, error));
    QVERIFY(error.isEmpty());
    QVERIFY(traffic.cars().empty());
  }
};

QTEST_GUILESS_MAIN(RoadNetworkTests)

#include "roads_tests.moc"
