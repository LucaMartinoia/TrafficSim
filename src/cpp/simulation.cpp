#include <QDebug>

#include "simulation.h"

Simulation::Simulation()
{
  m_timer.setInterval(static_cast<int>(m_timeStep * 1000.0));
  m_timer.setTimerType(Qt::PreciseTimer);
}

// Initialize the simulation
bool Simulation::start(const QJsonDocument& response, QString& error)
{
  // Build a fresh road graph and leave the current simulation untouched on failure
  if (!m_roadNetwork.buildRoadNetwork(response, error))
    return false;

  m_started = true;
  m_timer.start();

  // Log the prepared routing graphs
  const auto& nodes = m_roadNetwork.nodes();
  qInfo() << "Built road network with" << m_roadNetwork.nodes().size() << "nodes.";
  for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
  {
    const RoadNode& node = nodes[nodeIndex];
    qDebug().noquote() << QStringLiteral("  Node %1 (%2, %3)")
                              .arg(QString::number(static_cast<qulonglong>(nodeIndex)),
                                   QString::number(node.coordinate.latitude(), 'f', 6),
                                   QString::number(node.coordinate.longitude(), 'f', 6));
    for (const RoadEdge& edge : node.edges)
    {
      qDebug().noquote() << QStringLiteral("    -> %1: %2 m, %3 s")
                                .arg(QString::number(static_cast<qulonglong>(edge.destination)),
                                     QString::number(edge.length, 'f', 1),
                                     QString::number(edge.adaptiveTime, 'f', 1));
    }
  }

  return true;
}

// Single update step
bool Simulation::update(QString& error)
{
  // Refuse to advance until a road network has been loaded successfully.
  if (!m_started)
  {
    error = QStringLiteral("Simulation has not been started.");
    return false;
  }

  // Delegate stochastic car arrivals to the traffic model
  if (!m_traffic.spawnCars(m_roadNetwork, m_timeStep, error))
    return false;

  // Reset the occupancy list
  m_roadNetwork.resetOccupancy();

  // Advance all cars and update edge occupancy
  m_traffic.update(m_roadNetwork, m_timeStep);

  // Remove cars that have reached their destinations
  m_traffic.removeCars();

  // Recalculate travel times using the updated edge occupancy
  m_roadNetwork.updateTravelTimes();

  // Integrate simulation time.
  m_time += m_timeStep;

  return true;
}

// Convert the current active cars into coordinates consumable by QML
QVariantList Simulation::carPositions() const
{
  QVariantList positions;
  for (const Car& car : m_traffic.cars())
    positions.append(QVariant::fromValue(car.position));

  return positions;
}

// Reset the simulation
void Simulation::clear()
{
  m_roadNetwork = RoadNetwork{};
  m_traffic = Traffic{};
  m_time = 0.0;
  m_started = false;
  m_timer.stop();
}

// Getter methods
double Simulation::timeStep() const { return m_timeStep; }
double Simulation::time() const { return m_time; }
QTimer& Simulation::timer() { return m_timer; }
Traffic& Simulation::traffic() { return m_traffic; }