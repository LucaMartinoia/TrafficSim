#include "overpassclient.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

// Define the shared endpoint once outside the class declaration
const QUrl OverpassClient::ENDPOINT(QStringLiteral("https://overpass-api.de/api/interpreter"));

// Member-initializer list initializes the base and members before the constructor body
OverpassClient::OverpassClient(QObject* parent)
    : QObject(parent), m_networkAccessManager(new QNetworkAccessManager(this))
{
  // &Class::method is a type-safe pointer-to-member used by Qt connect()
  connect(m_networkAccessManager, &QNetworkAccessManager::finished, this,
          &OverpassClient::handleApiResponse);
}

bool OverpassClient::isOverpassResponse(const QJsonDocument& document)
{
  // Check the expected response structure as well as valid JSON syntax
  return document.isObject() && document.object().value(QStringLiteral("elements")).isArray() &&
         !document.object().contains(QStringLiteral("remark"));
}

// ClassName::methodName associates this definition with its header declaration
void OverpassClient::requestDefaultRoads()
{
  // Adjacent string literals join together; QStringLiteral creates a QString
  requestQuery(QStringLiteral("[out:json][timeout:15];\n"
                              "way['highway'~'^(motorway|primary|secondary|tertiary|residential)$']"
                              "(44.395,8.955,44.400,8.960);\n"
                              "out geom;"));
}

void OverpassClient::requestRoadsInBoundingBox(double south, double west, double north, double east)
{
  // Validate coordinates before building a remote query
  if (south < -90.0 || north > 90.0 || west < -180.0 || east > 180.0 || south >= north ||
      west >= east)
  {
    // emit notifies connected objects; return prevents sending invalid input
    emit requestFailed(QString(), QStringLiteral("Invalid bounding box."));
    return;
  }

  // arg() replaces placeholders; fixed precision keeps cache keys consistent
  const QString query =
      QStringLiteral(
          "[out:json][timeout:15];\n"
          "way['highway'~'^(motorway|primary|secondary|tertiary|residential)$'](%1,%2,%3,%4);\n"
          "out geom;")
          .arg(QString::number(south, 'f', 6), QString::number(west, 'f', 6),
               QString::number(north, 'f', 6), QString::number(east, 'f', 6));
  requestQuery(query);
}

QString OverpassClient::cacheFilePath(const QString& query) const
{
  // Use writable app data and hash the query for a unique cache filename
  const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (appDataLocation.isEmpty())
    return {};

  const QString cacheDirectory = appDataLocation + QStringLiteral("/roads_data");
  const QByteArray queryHash =
      QCryptographicHash::hash(query.toUtf8(), QCryptographicHash::Sha256).toHex();
  return cacheDirectory + QStringLiteral("/") + QString::fromLatin1(queryHash) +
         QStringLiteral(".json");
}

void OverpassClient::clearCache()
{
  // Remove the local road-response cache directory
  const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (appDataLocation.isEmpty())
    return;

  QDir cacheDirectory(appDataLocation + QStringLiteral("/roads_data"));

  if (cacheDirectory.exists())
    cacheDirectory.removeRecursively();
}

void OverpassClient::requestQuery(const QString& query)
{
  // Validate input and check the cache before contacting Overpass
  if (query.trimmed().isEmpty())
  {
    emit requestFailed(query, QStringLiteral("Overpass query cannot be empty."));
    return;
  }
  const QString cachePath = cacheFilePath(query);
  if (cachePath.isEmpty())
  {
    emit requestFailed(query,
                       QStringLiteral("Could not determine the application data directory."));
    return;
  }

  bool validCache = loadCachedResponse(query);
  if (validCache)
    return;

  // Configure an asynchronous HTTPS POST so the UI stays responsive
  QNetworkRequest request(ENDPOINT);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
  request.setRawHeader("User-Agent", "TrafficSim/0.1 (Qt Network client)");
  request.setRawHeader("Accept", "application/json");
  request.setTransferTimeout(45'000);

  // Percent-encode the query for the form-encoded HTTP body
  const QByteArray postData = QByteArrayLiteral("data=") + QUrl::toPercentEncoding(query);

  // Store the query on its reply so the handler can match each result
  QNetworkReply* reply = m_networkAccessManager->post(request, postData);

  reply->setProperty("query", query);
}

bool OverpassClient::loadCachedResponse(const QString& query)
{
  // Parse and validate cached JSON before treating the file as a cache hit
  const QString cachePath = cacheFilePath(query);
  QFile cacheFile(cachePath);

  if (!cacheFile.exists())
    return false;

  if (!cacheFile.open(QIODevice::ReadOnly))
  {
    qWarning().noquote() << "Could not read Overpass cache:" << cachePath
                         << cacheFile.errorString();
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument cachedResponse = QJsonDocument::fromJson(cacheFile.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !isOverpassResponse(cachedResponse))
  {
    const QString error =
        parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("Cache does not contain an Overpass elements array.");

    qWarning().noquote() << "Ignoring invalid Overpass cache:" << cachePath << error;
    return false;
  }

  qInfo().noquote() << "Using cached Overpass response:" << cachePath;
  emit requestSucceeded(query, cachedResponse);
  return true;
}

void OverpassClient::handleApiResponse(QNetworkReply* reply)
{
  // Retrieve this reply's query, including when requests overlap
  const QString query = reply->property("query").toString();
  const QString cachePath = cacheFilePath(query);

  const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  // Check network and HTTP errors; deleteLater() safely defers reply cleanup
  if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300)
  {
    QString error = reply->errorString();

    if (statusCode != 0)
      error = QStringLiteral("HTTP %1: %2").arg(statusCode).arg(error);

    reply->deleteLater();
    emit requestFailed(query, error);
    return;
  }

  const QByteArray responseBody = reply->readAll();
  QJsonParseError parseError;
  const QJsonDocument response = QJsonDocument::fromJson(responseBody, &parseError);

  // Cache and emit only a valid Overpass response
  if (parseError.error != QJsonParseError::NoError || !isOverpassResponse(response))
  {
    const QString detail =
        parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("Response does not contain an Overpass elements array.");

    reply->deleteLater();
    emit requestFailed(query, QStringLiteral("Invalid Overpass response: %1").arg(detail));
    return;
  }

  cacheResponse(query, responseBody);

  reply->deleteLater();
  emit requestSucceeded(query, response);
}

void OverpassClient::cacheResponse(const QString& query, const QByteArray& responseBody)
{
  // QSaveFile replaces the cache on commit, avoiding partial files
  const QString cachePath = cacheFilePath(query);
  if (cachePath.isEmpty())
  {
    qWarning() << "Could not determine cache file path.";
    return;
  }

  // Create the cache folder on the first successful request
  const QString cacheDirectory = QFileInfo(cachePath).absolutePath();
  if (!QDir().mkpath(cacheDirectory))
  {
    qWarning().noquote() << "Could not create cache directory:" << cacheDirectory;
    return;
  }

  QSaveFile cacheFile(cachePath);

  if (!cacheFile.open(QIODevice::WriteOnly))
  {
    qWarning().noquote() << "Could not open Overpass cache for writing:" << cachePath
                         << cacheFile.errorString();
    return;
  }

  if (cacheFile.write(responseBody) != responseBody.size())
  {
    qWarning().noquote() << "Could not write Overpass cache:" << cachePath
                         << cacheFile.errorString();
    cacheFile.cancelWriting();
    return;
  }

  if (!cacheFile.commit())
  {
    qWarning().noquote() << "Could not commit Overpass cache:" << cachePath
                         << cacheFile.errorString();
    return;
  }

  qInfo().noquote() << "Cached Overpass response:" << cachePath;
}