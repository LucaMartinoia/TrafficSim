#include <QDebug>

#include "simulation.h"

void Simulation::clear()
{
  m_traffic.clear();
  m_time = 0.0;
}

bool Simulation::start(const QJsonDocument& response, QString& error)
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
                                     QString::number(edge.travelTime, 'f', 1));
    }
  }

  return true;
}

bool Simulation::step(QString& error)
{
  // Refuse to advance until a road network has been loaded successfully.
  if (!m_started)
  {
    error = QStringLiteral("Simulation has not been started.");
    return false;
  }

  // Increment integrated time only after traffic advances successfully.
  if (!m_traffic.advance(m_roadNetwork, m_timeStep, error))
    return false;
  m_time += m_timeStep;
  return true;
}

double Simulation::timeStep() const { return m_timeStep; }

double Simulation::time() const { return m_time; }