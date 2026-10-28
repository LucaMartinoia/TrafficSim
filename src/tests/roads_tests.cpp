#include <cmath>
#include <vector>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>

#include "OSMclient.h"
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
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error),
             qPrintable(error));
    QCOMPARE(network.nodes().size(), std::size_t(5));

    // Verify full-graph edge direction, speed metadata, and computed travel time.
    const std::vector<RoadNode>& nodes = network.nodes();
    QVERIFY(nodes[0].edges.size() == 1);
    QCOMPARE(nodes[0].edges[0].destination, std::size_t(1));
    QCOMPARE(nodes[0].edges[0].maxVelocity, 10.0);
    QVERIFY(nodes[0].edges[0].travelTime > 0.0);
    QVERIFY(std::abs(nodes[0].edges[0].travelTime - nodes[0].edges[0].length / 10.0) < 0.001);
    QVERIFY(nodes[1].edges.size() == 2);
    QCOMPARE(nodes[1].edges[1].destination, std::size_t(2));
    QCOMPARE(nodes[2].edges[2].destination, std::size_t(4));
    QVERIFY(std::abs(nodes[2].edges[2].maxVelocity - 30.0 * 0.44704) < 0.000001);
    QVERIFY(nodes[4].edges.empty());

    // A successful replacement parse replaces the current graph.
    const QByteArray emptyNetworkJson = R"json({"elements": []})json";
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(emptyNetworkJson), error),
             qPrintable(error));
    QVERIFY(network.nodes().empty());
  }

  void defaultOverpassQueryBuildsRoadNetworks()
  {
    // Observe both outcomes from the real default query, including its cache path.
    OSMClient client;
    QSignalSpy succeededSpy(&client, &OSMClient::requestSucceeded);
    QSignalSpy failedSpy(&client, &OSMClient::requestFailed);
    QVERIFY(succeededSpy.isValid());
    QVERIFY(failedSpy.isValid());

    // Start the live request and allow time for the network timeout/retry handling.
    client.requestDefault();
    QTRY_VERIFY_WITH_TIMEOUT(succeededSpy.count() > 0 || failedSpy.count() > 0, 65000);

    if (failedSpy.count() > 0)
    {
      const QString error = failedSpy.takeFirst().at(1).toString();
      QFAIL(qPrintable(QStringLiteral("Default Overpass query failed: %1").arg(error)));
    }

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

  void simulationStartsAndIntegratesTime()
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
    QVERIFY(!simulation.step(error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(simulation.time(), 0.0);

    QVERIFY2(simulation.start(QJsonDocument::fromJson(json), error), qPrintable(error));
    QCOMPARE(simulation.time(), 0.0);
    QVERIFY(simulation.timeStep() > 0.0);

    // A single explicit step advances integrated time without starting a loop.
    QVERIFY2(simulation.step(error), qPrintable(error));
    QCOMPARE(simulation.time(), simulation.timeStep());
  }

  void trafficUsesMetersPerSecond()
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

    // Parse the road and create a car whose requested speed exceeds the road limit.
    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error),
             qPrintable(error));
    Traffic traffic;
    QVERIFY2(traffic.addCar(network, {0, 1}, 20.0, error), qPrintable(error));
    QCOMPARE(traffic.cars().front().velocity, 10.0);

    // One second at 10 m/s advances the car by 10 meters along the segment.
    QVERIFY2(traffic.advance(network, 1.0, error), qPrintable(error));
    QCOMPARE(traffic.cars().front().distanceAlongSegment, 10.0);
    QVERIFY(!traffic.cars().front().arrived);
  }
};

QTEST_GUILESS_MAIN(RoadNetworkTests)

#include "roads_tests.moc"
