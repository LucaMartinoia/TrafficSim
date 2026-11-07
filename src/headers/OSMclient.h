#pragma once

#include <array>
#include <utility>

#include <QGeoCoordinate>
#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

// Forward declarations avoid including the full networking headers here
class QNetworkAccessManager;
class QNetworkReply;

// QObject provides signals and Qt parent-child ownership
class OSMClient : public QObject
{
  Q_OBJECT

public:
  // explicit prevents implicit conversion; parent follows Qt ownership conventions
  explicit OSMClient(QObject* parent = nullptr);

  // Convenience methods for the fixed query and a bounding-box query
  void requestDefault();
  void requestRoadsBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox);
  void cancelPendingRequests();
  bool clearCache();

signals:
  // Keep the request details internal and expose only the parsed response
  void requestSucceeded(const QJsonDocument& response);

private:
  // Private helpers keep response handling and caching inside the client
  static const std::array<QUrl, 3> ENDPOINTS;
  static const QString WAY_TYPES;

  static bool isValidResponse(const QJsonDocument& document);
  void requestQuery(const QString& query);
  void sendQueryToEndpoint(const QString& query, std::size_t endpointIndex);
  bool loadCache(const QString& query);
  void saveCache(const QString& query, const QByteArray& responseBody);
  void handleResponse(QNetworkReply* reply);
  QString cacheFilePath(const QString& query) const;

  // QObject parent ownership deletes the manager with this client
  QNetworkAccessManager* m_networkAccessManager;
  QPointer<QNetworkReply> m_activeReply;
};