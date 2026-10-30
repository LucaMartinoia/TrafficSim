#include <algorithm>
#include <cmath>
#include <limits>

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

// Compute distance between two coordinates
double distance(const QGeoCoordinate& start, const QGeoCoordinate& end)
{
  // Reject invalid coordinates explicitly instead of returning a misleading distance
  if (!start.isValid() || !end.isValid())
    return std::numeric_limits<double>::quiet_NaN();

  return start.distanceTo(end);
}
} // namespace GeoUtils
