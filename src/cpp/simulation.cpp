#include <QDebug>

#include "simulation.h"

// Initialize the simulation
bool Simulation::init(const QJsonDocument& response, QString& error)
{
  // Build a fresh road graph and leave the current simulation untouched on failure
  if (!m_roadNetwork.buildRoadNetwork(response, error))
    return false;

  m_started = true;
  // Log the prepared routing graphs
  std::vector<RoadNode> nodes = m_roadNetwork.nodes();
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

  // Spawn until the requested active population is reached.
  while (m_traffic.activeCarCount() < 100)
  {
    // TODO: it can't never return false
    if (!m_traffic.addCar(m_roadNetwork, error))
      break;
  }

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

// Reset the simulation
void Simulation::clear()
{
  m_roadNetwork = RoadNetwork{};
  m_traffic = Traffic{};
  m_time = 0.0;
  m_started = false;
}

// Getter methods
double Simulation::timeStep() const { return m_timeStep; }
double Simulation::time() const { return m_time; }