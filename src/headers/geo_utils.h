#pragma once

#include <QGeoCoordinate>

namespace GeoUtils
{
// Check that a bounding box contains finite, ordered geographic coordinates
bool isValidBBox(double south, double west, double north, double east);

// Return the geodesic distance between coordinates in meters
double distance(const QGeoCoordinate& start, const QGeoCoordinate& end);

// Return a point on the geodesic segment, clamped to the segment's endpoints
QGeoCoordinate positionAlongSegment(const QGeoCoordinate& start, const QGeoCoordinate& end,
                                    double distanceFromStart);
} // namespace GeoUtils
