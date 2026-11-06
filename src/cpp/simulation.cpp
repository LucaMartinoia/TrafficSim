#include <QDebug>

#include "simulation.h"

Simulation::Simulation(QObject* parent) : QObject(parent)
{
  // Keep the timer-driven simulation lifecycle inside the simulation engine
  connect(&m_timer, &QTimer::timeout, this, &Simulation::update);
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

  return true;
}

// Advance one simulation step and notify observers of its outcome
void Simulation::update()
{
  QString error;

  // Refuse to advance until a road network has been loaded successfully.
  if (!m_started)
  {
    error = QStringLiteral("Simulation has not been started.");
    emit updateFailed(error);
    return;
  }

  // Delegate stochastic car arrivals to the traffic model
  if (!m_traffic.spawnCars(m_roadNetwork, m_timeStep, error))
  {
    emit updateFailed(error);
    return;
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

  emit carsUpdated(carPositions());
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
  m_roadNetwork.clear();
  m_traffic = Traffic{};
  m_time = 0.0;
  m_started = false;
  m_timer.stop();
}

// Getter methods
double Simulation::timeStep() const { return m_timeStep; }
double Simulation::time() const { return m_time; }
Traffic& Simulation::traffic() { return m_traffic; }
RoadNetwork& Simulation::roadNetwork() { return m_roadNetwork; }