#include <algorithm>
#include <cmath>
#include <limits>

#include "geo_utils.h"

namespace GeoUtils
{
bool isValidBBox(double south, double west, double north, double east)
{
  // Validate finite coordinates, geographic ranges, and strict bounds ordering
  return std::isfinite(south) && std::isfinite(west) && std::isfinite(north) &&
         std::isfinite(east) && south >= -90.0 && north <= 90.0 && west >= -180.0 &&
         east <= 180.0 && south < north && west < east;
}

double distance(const QGeoCoordinate& start, const QGeoCoordinate& end)
{
  // Reject invalid coordinates explicitly instead of returning a misleading distance
  if (!start.isValid() || !end.isValid())
    return std::numeric_limits<double>::quiet_NaN();

  return start.distanceTo(end);
}

QGeoCoordinate positionAlongSegment(const QGeoCoordinate& start, const QGeoCoordinate& end,
                                    double distanceFromStart)
{
  // Reject invalid inputs and keep the requested position within the segment
  if (!start.isValid() || !end.isValid() || !std::isfinite(distanceFromStart))
    return QGeoCoordinate();

  const double segmentLength = distance(start, end);
  if (segmentLength <= 0.0)
    return start;

  const double clampedDistance = std::max(0.0, std::min(segmentLength, distanceFromStart));
  return start.atDistanceAndAzimuth(clampedDistance, start.azimuthTo(end));
}
} // namespace GeoUtils
