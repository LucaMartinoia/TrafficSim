#include <vector>

#include <QPersistentModelIndex>
#include <QSignalSpy>
#include <QtTest>

#include "cars_model.h"

class CarsModelTests : public QObject
{
  Q_OBJECT

private slots:
  void updatesPreserveContinuingCarRows()
  {
    // Load two identified cars and keep persistent indexes to their delegates
    Car firstCar;
    firstCar.id = 1;
    firstCar.position = QGeoCoordinate(44.0, 8.0);
    Car secondCar;
    secondCar.id = 2;
    secondCar.position = QGeoCoordinate(44.001, 8.001);
    std::vector<Car> initialCars{firstCar, secondCar};

    CarsModel model;
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(insertedSpy.isValid());
    QVERIFY(removedSpy.isValid());
    QVERIFY(changedSpy.isValid());

    model.synchronize(initialCars);
    QCOMPARE(model.rowCount(), 2);
    QPersistentModelIndex firstIndex(model.index(0, 0));
    QPersistentModelIndex secondIndex(model.index(1, 0));

    // Replace the source vector and verify the model follows its current Car objects
    std::vector<Car> movedCars = initialCars;
    movedCars[1].position = QGeoCoordinate(44.002, 8.002);
    model.synchronize(movedCars);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.constLast().at(0).value<QModelIndex>().row(), 1);
    QCOMPARE(changedSpy.constLast().at(1).value<QModelIndex>().row(), 1);
    QCOMPARE(model.data(secondIndex, CarsModel::PositionRole).value<QGeoCoordinate>(),
             movedCars[1].position);
    const double expectedDirection =
        initialCars[1].position.azimuthTo(movedCars[1].position);
    QCOMPARE(model.data(secondIndex, CarsModel::DirectionRole).toDouble(), expectedDirection);
    QCOMPARE(firstIndex.row(), 0);
    QCOMPARE(secondIndex.row(), 1);
    QCOMPARE(insertedSpy.count(), 2);

    // Remove one car and add another without invalidating the surviving car index
    Car thirdCar;
    thirdCar.id = 3;
    thirdCar.position = QGeoCoordinate(44.003, 8.003);
    Car movedSecondCar = movedCars[1];
    movedSecondCar.position = QGeoCoordinate(44.004, 8.004);
    std::vector<Car> finalCars{movedSecondCar, thirdCar};
    model.synchronize(finalCars);

    QCOMPARE(model.rowCount(), 2);
    QVERIFY(!firstIndex.isValid());
    QCOMPARE(secondIndex.row(), 0);
    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(changedSpy.constLast().at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(changedSpy.constLast().at(1).value<QModelIndex>().row(), 0);
    QCOMPARE(model.data(secondIndex, CarsModel::PositionRole).value<QGeoCoordinate>(),
             movedSecondCar.position);
    QCOMPARE(model.data(secondIndex, CarsModel::DirectionRole).toDouble(),
             movedCars[1].position.azimuthTo(movedSecondCar.position));
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(insertedSpy.count(), 3);
    QCOMPARE(model.roleNames().value(CarsModel::PositionRole), QByteArrayLiteral("position"));
    QCOMPARE(model.roleNames().value(CarsModel::DirectionRole), QByteArrayLiteral("direction"));
  }

  void clearRemovesVisibleCars()
  {
    // Clearing the simulation model intentionally removes its remaining delegates
    Car car;
    car.id = 10;
    car.position = QGeoCoordinate(44.0, 8.0);
    const std::vector<Car> cars{car};

    CarsModel model;
    model.synchronize(cars);
    QPersistentModelIndex carIndex(model.index(0, 0));
    model.clear();

    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!carIndex.isValid());
  }
};

QTEST_GUILESS_MAIN(CarsModelTests)
#include "cars_model_tests.moc"
