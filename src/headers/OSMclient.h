#pragma once

#include <QJsonDocument>
#include <QObject>
#include <QString>
#include <QUrl>

// Forward declarations avoid including the full networking headers here
class QNetworkAccessManager;
class QNetworkReply;

// QObject provides signals and Qt parent-child ownership; Q_OBJECT enables its meta-object features
class OSMClient : public QObject
{
  Q_OBJECT

public:
  // explicit prevents implicit conversion; parent follows Qt ownership conventions
  explicit OSMClient(QObject* parent = nullptr);

  // Convenience methods for the fixed query and a bounding-box query
  void requestDefault();
  void requestRoadsBBox(double south, double west, double north, double east);

signals:
  // Qt generates signal implementations; callers connect to receive results
  void requestSucceeded(const QString& query, const QJsonDocument& response);
  void requestFailed(const QString& query, const QString& error);

private:
  // Private helpers keep response handling and caching inside the client
  static const QUrl ENDPOINT;
  static const QString WAY_TYPES;

  static bool isValidResponse(const QJsonDocument& document);
  void requestQuery(const QString& query);
  void clearCache();
  bool loadCache(const QString& query);
  void handleResponse(QNetworkReply* reply);
  void saveCache(const QString& query, const QByteArray& responseBody);

  QString cacheFilePath(const QString& query) const;

  // QObject parent ownership deletes the manager with this client
  QNetworkAccessManager* m_networkAccessManager;
};