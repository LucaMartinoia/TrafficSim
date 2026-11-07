#include <cmath>
#include <utility>
#include <vector>

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtMath>
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
            "tags": {"highway": "residential", "maxspeed": "36", "lanes": "2"},
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
            "tags": {"highway": "residential", "oneway": "yes", "maxspeed": "30 mph", "lanes": "3"},
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
    QCOMPARE(nodes[0].edges[0].lanes, 2);
    QVERIFY(nodes[0].edges[0].minTime > 0.0);
    QVERIFY(std::abs(nodes[0].edges[0].minTime - nodes[0].edges[0].length / 10.0) < 0.001);
    QCOMPARE(nodes[0].edges[0].travelTime, nodes[0].edges[0].minTime);
    QVERIFY(nodes[1].edges.size() == 2);
    QCOMPARE(nodes[1].edges[1].destination, std::size_t(2));
    QCOMPARE(nodes[2].edges[2].destination, std::size_t(4));
    QVERIFY(std::abs(nodes[2].edges[2].maxVelocity - 30.0 * 0.44704) < 0.000001);
    QCOMPARE(nodes[2].edges[2].lanes, 3);
    QVERIFY(nodes[4].edges.empty());

    // Confirm occupancy adds delay above one car and that lane count moderates the increase.
    network.setRoutingMetric(RoutingMetric::AdaptiveTime);
    network.updateTravelTimes();
    QCOMPARE(nodes[0].edges[0].travelTime, nodes[0].edges[0].minTime);
    network.incrementOccupancy({0, 0});
    network.updateTravelTimes();
    QCOMPARE(nodes[0].edges[0].travelTime, nodes[0].edges[0].minTime);
    network.incrementOccupancy({0, 0});
    network.incrementOccupancy({0, 0});
    network.updateTravelTimes();
    QVERIFY(std::abs(nodes[0].edges[0].travelTime - nodes[0].edges[0].minTime * 2.0) < 0.001);
    network.incrementOccupancy({2, 2});
    network.incrementOccupancy({2, 2});
    network.incrementOccupancy({2, 2});
    network.updateTravelTimes();
    QVERIFY(std::abs(nodes[2].edges[2].travelTime - nodes[2].edges[2].minTime * (1.0 + 2.0 / 3.0)) <
            0.001);
    network.resetOccupancy();
    network.updateTravelTimes();
    QCOMPARE(nodes[0].edges[0].travelTime, nodes[0].edges[0].minTime);

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
    const std::pair<QGeoCoordinate, QGeoCoordinate> bbox{topLeft, bottomRight};
    QVERIFY(GeoUtils::isValidBBox(bbox));

    // Verify a one-degree square at the equator uses the spherical area calculation
    const QGeoCoordinate equatorTopLeft(1.0, 0.0);
    const QGeoCoordinate equatorBottomRight(0.0, 1.0);
    const double expectedArea =
        6371000.0 * 6371000.0 * qDegreesToRadians(1.0) * std::sin(qDegreesToRadians(1.0));
    const std::pair<QGeoCoordinate, QGeoCoordinate> equatorBBox{equatorTopLeft,
                                                                equatorBottomRight};
    QVERIFY(std::abs(GeoUtils::area(equatorBBox) - expectedArea) <
            expectedArea * 0.000001);

    // Reject invalid coordinates and corners with reversed ordering
    QVERIFY(!GeoUtils::isValidBBox({QGeoCoordinate(91.0, 8.0), bottomRight}));
    QVERIFY(!GeoUtils::isValidBBox({bottomRight, topLeft}));
    QVERIFY(std::isnan(GeoUtils::area({bottomRight, topLeft})));
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

  void simulationMovesCarsOnFixedSteps()
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
    QSignalSpy roadsReceivedSpy(&simulation, &Simulation::roadsReceived);
    QSignalSpy carsUpdatedSpy(&simulation, &Simulation::carsUpdated);
    QSignalSpy updateFailedSpy(&simulation, &Simulation::updateFailed);
    QVERIFY(roadsReceivedSpy.isValid());
    QVERIFY(carsUpdatedSpy.isValid());
    QVERIFY(updateFailedSpy.isValid());

    simulation.update();
    QCOMPARE(updateFailedSpy.count(), 1);
    QCOMPARE(simulation.time(), 0.0);

    const QGeoCoordinate topLeft(44.01, 8.0);
    const QGeoCoordinate bottomRight(44.0, 8.01);
    const std::pair<QGeoCoordinate, QGeoCoordinate> bbox{topLeft, bottomRight};
    simulation.traffic().setTargetFromBBox(bbox, 0);
    QString error;
    QVERIFY2(simulation.start(QJsonDocument::fromJson(json), error), qPrintable(error));
    QCOMPARE(roadsReceivedSpy.count(), 1);
    QVERIFY(!roadsReceivedSpy.constLast().at(0).toList().isEmpty());
    QCOMPARE(simulation.time(), 0.0);
    QCOMPARE(simulation.timeStep(), 0.05);

    // Verify the box determines a target, while arrivals grow stochastically toward it.
    const int targetCars = simulation.traffic().target();
    const int expectedCars =
        static_cast<int>(std::round(GeoUtils::area(bbox) * 2.0e-4 * 0.7 * 0.1));
    QCOMPARE(targetCars, expectedCars);
    // Add one deterministic test car and disable random arrivals for this lifecycle check
    simulation.traffic().setTarget(0);
    QVERIFY2(simulation.traffic().addCar(simulation.roadNetwork(), error), qPrintable(error));
    simulation.update();
    QCOMPARE(simulation.time(), 0.05);
    QCOMPARE(carsUpdatedSpy.count(), 1);
    const QVariantList initialPositions = simulation.carPositions();
    QVERIFY(!initialPositions.isEmpty());

    simulation.update();
    QCOMPARE(simulation.time(), 0.1);
    QCOMPARE(carsUpdatedSpy.count(), 2);
    const QVariantList movedPositions = simulation.carPositions();
    QVERIFY(!movedPositions.isEmpty());
    const QGeoCoordinate initialPosition = initialPositions.first().value<QGeoCoordinate>();
    const QGeoCoordinate movedPosition = movedPositions.first().value<QGeoCoordinate>();
    QVERIFY(initialPosition.distanceTo(movedPosition) > 0.0);

    // Clear the simulation and verify that it cannot advance without a network.
    simulation.clear();
    QCOMPARE(roadsReceivedSpy.count(), 2);
    QVERIFY(roadsReceivedSpy.constLast().at(0).toList().isEmpty());
    QCOMPARE(carsUpdatedSpy.count(), 3);
    QVERIFY(carsUpdatedSpy.constLast().at(0).toList().isEmpty());
    simulation.update();
    QCOMPARE(updateFailedSpy.count(), 2);
    QCOMPARE(simulation.time(), 0.0);
  }

  void trafficCreatesCarUsingReachableRoute()
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

    // Parse a directed road and verify that a car receives a route on it.
    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error), qPrintable(error));
    Traffic traffic;
    traffic.setTarget(4);
    QCOMPARE(traffic.target(), 4);
    traffic.setTarget(-1);
    QCOMPARE(traffic.target(), 0);
    QVERIFY(traffic.setTargetScale(0.1));
    QVERIFY(traffic.setTargetScale(10.0));
    QVERIFY(!traffic.setTargetScale(0.0));
    QVERIFY(!traffic.setTargetScale(10.1));
    QCOMPARE(traffic.target(), 0);
    error = QStringLiteral("previous error");
    QVERIFY2(traffic.addCar(network, error), qPrintable(error));
    QVERIFY(error.isEmpty());
    QCOMPARE(network.nodes()[0].edges[0].lanes, 1);
    QCOMPARE(traffic.cars().size(), std::size_t(1));
    QCOMPARE(traffic.cars().front().route.size(), std::size_t(1));
  }

  void trafficKeepsPositivePoissonRateAboveTarget()
  {
    // Build a reachable road so the test can place the population above its target
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

    RoadNetwork network;
    QString error;
    QVERIFY2(network.buildRoadNetwork(QJsonDocument::fromJson(json), error), qPrintable(error));

    // Five cars above the target exercise the lower-bound arrival rate
    Traffic traffic;
    traffic.setTarget(1);
    for (int car = 0; car < 6; ++car)
      QVERIFY2(traffic.addCar(network, error), qPrintable(error));

    // The positive rate should sample safely even when the population exceeds its target
    QCOMPARE(traffic.cars().size(), std::size_t(6));
    QVERIFY2(traffic.createCars(network, 0.05, error), qPrintable(error));
    QVERIFY(traffic.cars().size() >= std::size_t(6));

    // A zero timestep is a no-op and must not construct the distribution
    const std::size_t carsBeforeZeroStep = traffic.cars().size();
    QVERIFY2(traffic.createCars(network, 0.0, error), qPrintable(error));
    QCOMPARE(traffic.cars().size(), carsBeforeZeroStep);
  }
};

QTEST_GUILESS_MAIN(RoadNetworkTests)

#include "roads_tests.moc"
