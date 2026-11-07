#pragma once

#include <utility>

#include <QGeoCoordinate>

namespace GeoUtils
{
// Check if the map center coordinate and zoom level are valid
bool isValidMapView(const QGeoCoordinate& center, int zoomLevel);

// Check that the top-left and bottom-right coordinates form a valid bounding box
bool isValidBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox);

// Return the spherical area of a bounding box in square meters
double area(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox);

// Return the geodesic distance between coordinates in meters
double distance(const QGeoCoordinate& start, const QGeoCoordinate& end);
} // namespace GeoUtils
