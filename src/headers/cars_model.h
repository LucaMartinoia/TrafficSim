#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QGeoCoordinate>
#include <QtGlobal>

#include "traffic.h"

struct CarsData
{
  quint64 id = 0;
  QGeoCoordinate position;
  double direction = 0.0;
};

class CarsModel : public QAbstractListModel
{
  Q_OBJECT

public:
  enum Roles
  {
    // Qt::UserRole are the enums used by Qt, so we start the enum one after
    PositionRole = Qt::UserRole + 1,
    DirectionRole
  };

  explicit CarsModel(QObject* parent = nullptr);

  // Methods to override the abstract list model functions
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  // New methods
  void synchronize(const std::vector<Car>& cars);
  void clear();

private:
  // We keep a list of cars sorted for QML
  std::vector<CarsData> m_cars;
};
