#include <cmath>
#include <limits>

#include <QtMath>

#include "geo_utils.h"

namespace GeoUtils
{
// Check if valid map view
bool isValidMapView(const QGeoCoordinate& center, int zoomLevel)
{
  return center.isValid() && zoomLevel >= 0 && zoomLevel <= 22;
}

// Check if valid bounding box
bool isValidBBox(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  // Validate geographic coordinates and strict top-left to bottom-right ordering
  return topLeft.isValid() && bottomRight.isValid() &&
         topLeft.latitude() > bottomRight.latitude() &&
         topLeft.longitude() < bottomRight.longitude();
}

// Compute the spherical area of a longitude-latitude rectangle
double area(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  // Reject invalid or incorrectly ordered bounds with an explicit invalid result
  if (!isValidBBox(topLeft, bottomRight))
    return std::numeric_limits<double>::quiet_NaN();

  // Convert the coordinate differences into radians for the spherical area formula
  constexpr double earthRadius = 6371000.0;
  const double longitudeSpan =
      qDegreesToRadians(bottomRight.longitude() - topLeft.longitude());
  const double northLatitude = qDegreesToRadians(topLeft.latitude());
  const double southLatitude = qDegreesToRadians(bottomRight.latitude());

  return earthRadius * earthRadius * longitudeSpan *
         (std::sin(northLatitude) - std::sin(southLatitude));
}

// Compute distance between two coordinates
double distance(const QGeoCoordinate& start, const QGeoCoordinate& end)
{
  // Reject invalid coordinates explicitly instead of returning a misleading distance
  if (!start.isValid() || !end.isValid())
    return std::numeric_limits<double>::quiet_NaN();

  return start.distanceTo(end);
}
} // namespace GeoUtils
