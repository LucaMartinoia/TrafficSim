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

#include "OSMclient.h"
#include "geo_utils.h"

// Define public Overpass instances in preferred order
const std::array<QUrl, 3> OSMClient::ENDPOINTS = {
    {QUrl(QStringLiteral("https://overpass-api.de/api/interpreter")),
     QUrl(QStringLiteral("https://overpass.private.coffee/api/interpreter")),
     QUrl(QStringLiteral("https://maps.mail.ru/osm/tools/overpass/api/interpreter"))}};
const QString OSMClient::WAY_TYPES(
    "way['highway'~'^(motorway|trunk|primary|secondary|tertiary|unclassified|residential|motorway_"
    "link|trunk_link|primary_link|secondary_link|tertiary_link)$']");

// Member-initializer list initializes the base and members before the constructor body
OSMClient::OSMClient(QObject* parent)
    : QObject(parent), m_networkAccessManager(new QNetworkAccessManager(this))
{
  // &Class::method is a type-safe pointer-to-member used by Qt connect()
  connect(m_networkAccessManager, &QNetworkAccessManager::finished, this,
          &OSMClient::handleResponse);
}

// Check the expected response structure as well as valid JSON syntax
bool OSMClient::isValidResponse(const QJsonDocument& document)
{
  return document.isObject() && document.object().value(QStringLiteral("elements")).isArray() &&
         !document.object().contains(QStringLiteral("remark"));
}

// ClassName::methodName associates this definition with its header declaration
void OSMClient::requestDefault()
{
  // Adjacent string literals join together; QStringLiteral creates a QString
  requestQuery(QStringLiteral("[out:json][timeout:25];\n"
                              "%1(44.395,8.955,44.400,8.961);\n"
                              "out geom;")
                   .arg(WAY_TYPES));
}

// Request road data within a Bounding Box
void OSMClient::requestRoadsBBox(const QGeoCoordinate& topLeft, const QGeoCoordinate& bottomRight)
{
  // Validate coordinates before building a remote query
  if (!GeoUtils::isValidBBox(topLeft, bottomRight))
  {
    // emit notifies connected objects; return prevents sending invalid input
    emit requestFailed(QString(), QStringLiteral("Invalid bounding box."));
    return;
  }

  // arg() replaces placeholders; fixed precision keeps cache keys consistent
  const QString query = QStringLiteral("[out:json][timeout:15];\n"
                                       "%5(%1,%2,%3,%4);\n"
                                       "out geom;")
                            .arg(QString::number(bottomRight.latitude(), 'f', 6),
                                 QString::number(topLeft.longitude(), 'f', 6),
                                 QString::number(topLeft.latitude(), 'f', 6),
                                 QString::number(bottomRight.longitude(), 'f', 6), WAY_TYPES);
  requestQuery(query);
}

// General query handler
void OSMClient::requestQuery(const QString& query)
{
  // Validate input and check the cache before contacting Overpass
  if (query.trimmed().isEmpty())
  {
    emit requestFailed(query, QStringLiteral("Overpass query cannot be empty."));
    return;
  }

  bool validCache = loadCache(query);
  if (validCache)
    return;

  // Start with the first instance; each failure advances to the next asynchronously
  sendQueryToEndpoint(query, 0);
}

// Send an HTTPS POST request to an Overpass endpoint
void OSMClient::sendQueryToEndpoint(const QString& query, std::size_t endpointIndex)
{
  // Configure an asynchronous HTTPS POST so the UI stays responsive
  qInfo().noquote() << "Sending HTTPS request to Overpass API:"
                    << ENDPOINTS[endpointIndex].toString();
  QNetworkRequest request(ENDPOINTS[endpointIndex]);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
  request.setRawHeader("User-Agent", "TrafficSim/0.1 (Qt Network client)");
  request.setRawHeader("Accept", "application/json");
  request.setTransferTimeout(25'000);

  // Percent-encode the query for the form-encoded HTTP body
  const QByteArray postData = QByteArrayLiteral("data=") + QUrl::toPercentEncoding(query);

  // Store request state on the reply so overlapping queries remain independent
  QNetworkReply* reply = m_networkAccessManager->post(request, postData);
  reply->setProperty("query", query);
  reply->setProperty("endpointIndex", static_cast<int>(endpointIndex));
}

// Compute file path
QString OSMClient::cacheFilePath(const QString& query) const
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

// Remove the local road-response cache directory
bool OSMClient::clearCache()
{
  const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (appDataLocation.isEmpty())
  {
    qWarning() << "Could not locate application data directory to clear OSM cache";
    return false;
  }

  QDir cacheDirectory(appDataLocation + QStringLiteral("/roads_data"));

  if (!cacheDirectory.exists())
    return true;

  if (!cacheDirectory.removeRecursively())
  {
    qWarning() << "Could not clear OSM cache directory:" << cacheDirectory.path();
    return false;
  }
  return true;
}

// Load saved cache
bool OSMClient::loadCache(const QString& query)
{
  // Parse and validate cached JSON before treating the file as a cache hit
  const QString cachePath = cacheFilePath(query);
  QFile cacheFile(cachePath);

  if (!cacheFile.exists())
  {
    qWarning().noquote() << "Could not find Overpass cache:" << cachePath;
    return false;
  }

  if (!cacheFile.open(QIODevice::ReadOnly))
  {
    qWarning().noquote() << "Could not read Overpass cache:" << cachePath
                         << cacheFile.errorString();
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument cachedResponse = QJsonDocument::fromJson(cacheFile.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !isValidResponse(cachedResponse))
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

// Save cache
void OSMClient::saveCache(const QString& query, const QByteArray& responseBody)
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

// Callback to the async HTTPS request
void OSMClient::handleResponse(QNetworkReply* reply)
{
  // Retrieve this reply's query, including when requests overlap
  const QString query = reply->property("query").toString();
  const std::size_t endpointIndex =
      static_cast<std::size_t>(reply->property("endpointIndex").toInt());

  const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  // Check network and HTTP errors; deleteLater() safely defers reply cleanup
  if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300)
  {
    QString error = reply->errorString();

    if (statusCode != 0)
      error = QStringLiteral("HTTP %1: %2").arg(statusCode).arg(error);

    // Log the failed instance and release its reply
    qWarning().noquote() << "Overpass endpoint failed:" << ENDPOINTS[endpointIndex].toString()
                         << error;
    reply->deleteLater();

    // Cycle through all instances forever until one returns a valid response
    const std::size_t nextEndpoint = (endpointIndex + 1) % ENDPOINTS.size();
    sendQueryToEndpoint(query, nextEndpoint);
    return;
  }

  const QByteArray responseBody = reply->readAll();
  QJsonParseError parseError;
  const QJsonDocument response = QJsonDocument::fromJson(responseBody, &parseError);

  // Cache and emit only a valid Overpass response
  if (parseError.error != QJsonParseError::NoError || !isValidResponse(response))
  {
    const QString detail =
        parseError.error != QJsonParseError::NoError
            ? parseError.errorString()
            : QStringLiteral("Response does not contain an Overpass elements array.");

    // Log the invalid response and try the next instance
    qWarning().noquote() << "Overpass endpoint returned invalid data:"
                         << ENDPOINTS[endpointIndex].toString() << detail;
    reply->deleteLater();

    // Cycle through all instances until one returns a valid response
    const std::size_t nextEndpoint = (endpointIndex + 1) % ENDPOINTS.size();
    sendQueryToEndpoint(query, nextEndpoint);
    return;
  }

  saveCache(query, responseBody);

  reply->deleteLater();
  emit requestSucceeded(query, response);
}
