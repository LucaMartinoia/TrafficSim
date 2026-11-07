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
void Simulation::start(const QJsonDocument& response)
{
  // Build a fresh road graph and leave the current simulation untouched on failure
  if (!m_roadNetwork.buildRoadNetwork(response))
    return;

  // Publish the loaded road network directly to observers such as the QML map
  emit roadsReceived(m_roadNetwork.roadsPolyline());

  // Set the simulation population from this request's bounding-box area
  m_traffic.setTargetFromBBox(m_bbox, m_roadNetwork.ways().size());

  m_started = true;
  m_timer.start();
}

// Advance one simulation step and notify observers of its outcome
void Simulation::update()
{
  // Delegate car arrivals to the traffic model
  m_traffic.createCars(m_roadNetwork, m_timeStep);

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

  if (++m_stepCount % 40 == 0) // 40 * 0.05 = 2 seconds
    qInfo() << "Active cars:" << m_traffic.cars().size();

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
  m_stepCount = 0;
  m_started = false;
  m_timer.stop();

  // Publish the cleared state
  emit roadsReceived({});
  emit carsUpdated({});
}

void Simulation::setBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox) { m_bbox = bbox; }

// Getter methods
double Simulation::timeStep() const { return m_timeStep; }
double Simulation::time() const { return m_time; }
Traffic& Simulation::traffic() { return m_traffic; }
RoadNetwork& Simulation::roadNetwork() { return m_roadNetwork; }