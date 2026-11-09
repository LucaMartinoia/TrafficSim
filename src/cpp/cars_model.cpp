#include <QSet>

#include "cars_model.h"

CarsModel::CarsModel(QObject* parent) : QAbstractListModel(parent) {}

// Override of the AstractListModel function: one row for active car
int CarsModel::rowCount(const QModelIndex& parent) const
{
  if (parent.isValid())
    return 0;

  return m_cars.size();
}

// Override of the AstractListModel function: return the i-th car position
QVariant CarsModel::data(const QModelIndex& index, int role) const
{
  if (!index.isValid() || index.row() < 0 || index.row() >= m_cars.size())
    return {};

  if (role == PositionRole)
    return QVariant::fromValue(m_cars[index.row()].position);
  if (role == DirectionRole)
    return m_cars[index.row()].direction;

  return {};
}

// Override of the AstractListModel function: return role-string name for QML
QHash<int, QByteArray> CarsModel::roleNames() const
{
  return {{PositionRole, QByteArrayLiteral("position")},
          {DirectionRole, QByteArrayLiteral("direction")}};
}

// Sync the simulation data with the QML representation
void CarsModel::synchronize(const std::vector<Car>& cars)
{
  // Collect the IDs that should remain in the model
  QSet<quint64> activeIds;
  activeIds.reserve(cars.size());
  for (const Car& car : cars)
    activeIds.insert(car.id);

  // Remove missing cars in contiguous ranges, from the end to preserve row indices
  int row = m_cars.size() - 1;
  while (row >= 0)
  {
    if (activeIds.contains(m_cars[row].id))
    {
      --row;
      continue;
    }

    const int lastRemovedRow = row;
    while (row >= 0 && !activeIds.contains(m_cars[row].id))
      --row;

    const int firstRemovedRow = row + 1;
    beginRemoveRows(QModelIndex(), firstRemovedRow, lastRemovedRow);
    m_cars.erase(m_cars.begin() + firstRemovedRow, m_cars.begin() + lastRemovedRow + 1);
    endRemoveRows();
  }

  // Update continuing positions at their actual rows and append newly spawned cars
  for (const Car& car : cars)
  {
    int existingRow = 0;
    while (existingRow < m_cars.size() && m_cars[existingRow].id != car.id)
      ++existingRow;

    if (existingRow == m_cars.size())
    {
      const int newRow = m_cars.size();
      beginInsertRows(QModelIndex(), newRow, newRow);
      m_cars.push_back({car.id, car.position, 0.0});
      endInsertRows();
      continue;
    }

    // Update rendering data and derive the heading from the previous position
    CarsData& carData = m_cars[existingRow];
    if (carData.position != car.position)
    {
      carData.direction = carData.position.azimuthTo(car.position);
      carData.position = car.position;
      const QModelIndex changedIndex = index(existingRow, 0);
      emit dataChanged(changedIndex, changedIndex, {PositionRole, DirectionRole});
    }
  }
}

void CarsModel::clear()
{
  if (m_cars.empty())
    return;

  beginResetModel();
  m_cars.clear();
  endResetModel();
}
