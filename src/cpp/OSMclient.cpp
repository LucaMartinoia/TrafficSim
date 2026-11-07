#include <QCryptographicHash>
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

/*
HTTPS REQUESTS
*/

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
void OSMClient::requestRoadsBBox(const std::pair<QGeoCoordinate, QGeoCoordinate>& bbox)
{
  // Validate coordinates before building a remote query
  if (!GeoUtils::isValidBBox(bbox))
  {
    qWarning() << "Cannot request road data for an invalid bounding box";
    return;
  }

  const QGeoCoordinate& topLeft = bbox.first;
  const QGeoCoordinate& bottomRight = bbox.second;

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
  // Supersede any previous request before serving the cache or starting a new chain
  cancelPendingRequests();

  if (loadCache(query))
    return;

  // Start with the first instance; each failure advances to the next asynchronously
  sendQueryToEndpoint(query, 0);
}

// Send an HTTPS POST request to an Overpass endpoint
void OSMClient::sendQueryToEndpoint(const QString& query, std::size_t endpointIndex)
{
  // Configure an asynchronous HTTPS POST so the UI stays responsive
  qInfo() << "Sending HTTPS request to Overpass API:" << ENDPOINTS[endpointIndex].toString();
  QNetworkRequest request(ENDPOINTS[endpointIndex]);
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/x-www-form-urlencoded"));
  request.setRawHeader("User-Agent", "TrafficSim/0.1 (Qt Network client)");
  request.setRawHeader("Accept", "application/json");
  request.setTransferTimeout(25'000);

  // Percent-encode the query for the form-encoded HTTP body
  const QByteArray postData = QByteArrayLiteral("data=") + QUrl::toPercentEncoding(query);

  // Store request state on the active reply for the retry chain
  QNetworkReply* reply = m_networkAccessManager->post(request, postData);
  reply->setProperty("query", query);
  reply->setProperty("endpointIndex", static_cast<int>(endpointIndex));
  m_activeReply = reply;
}

// Abort the active reply before its completion callback can continue the retry chain
void OSMClient::cancelPendingRequests()
{
  QNetworkReply* reply = m_activeReply.data();
  m_activeReply.clear();

  if (reply)
    reply->abort();
}

/*
CACHE DATA
*/

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
    return false;

  QDir cacheDirectory(appDataLocation + QStringLiteral("/roads_data"));
  return !cacheDirectory.exists() || cacheDirectory.removeRecursively();
}

// Load saved cache
bool OSMClient::loadCache(const QString& query)
{
  const QString cachePath = cacheFilePath(query);
  QFile cacheFile(cachePath);

  if (!cacheFile.open(QIODevice::ReadOnly))
  {
    qWarning() << "Could not load cached roads data:"
               << (cacheFile.exists() ? cacheFile.errorString()
                                      : QStringLiteral("File does not exist"));
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument cachedResponse = QJsonDocument::fromJson(cacheFile.readAll(), &parseError);

  if (parseError.error != QJsonParseError::NoError)
  {
    qWarning() << "Ignoring invalid cached roads data:" << cachePath << parseError.errorString();
    return false;
  }

  qInfo() << "Using cached roads data";

  emit requestSucceeded(cachedResponse);
  return true;
}

// Save cache
void OSMClient::saveCache(const QString& query, const QByteArray& responseBody)
{
  const QString cachePath = cacheFilePath(query);
  const QString cacheDirectory = QFileInfo(cachePath).absolutePath();

  QSaveFile cacheFile(cachePath);

  if (!cachePath.isEmpty() && QDir().mkpath(cacheDirectory) && cacheFile.open(QIODevice::WriteOnly))
  {
    if (cacheFile.write(responseBody) == responseBody.size() && cacheFile.commit())
    {
      qInfo() << "Roads data cache saved";
      return;
    }

    cacheFile.cancelWriting();
  }

  qWarning() << "Could not cache roads data:" << cachePath;
}

// Callback to the async HTTPS request
void OSMClient::handleResponse(QNetworkReply* reply)
{
  // Ignore replies canceled or superseded by a newer request
  if (m_activeReply.data() != reply)
  {
    reply->deleteLater();
    return;
  }

  // Clear the active reply before handling the result or advancing the retry chain
  m_activeReply.clear();

  // Retrieve this reply's query and endpoint for response handling
  const QString query = reply->property("query").toString();
  const std::size_t endpointIndex =
      static_cast<std::size_t>(reply->property("endpointIndex").toInt());

  const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  // Reject failed or closed replies before attempting to read their response body
  if (reply->error() != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300 ||
      !reply->isOpen() || !reply->isReadable())
  {
    QString error = reply->errorString();

    if (statusCode != 0)
      error = QStringLiteral("HTTP %1: %2").arg(statusCode).arg(error);
    else if (reply->error() == QNetworkReply::NoError)
      error = QStringLiteral("Response device closed before it could be read.");

    // Log the failed instance and release its reply
    qWarning() << "Overpass API endpoint failed:" << ENDPOINTS[endpointIndex].toString() << error;
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
    qWarning() << "Overpass API endpoint returned invalid data:"
               << ENDPOINTS[endpointIndex].toString() << detail;
    reply->deleteLater();

    // Cycle through all instances until one returns a valid response
    const std::size_t nextEndpoint = (endpointIndex + 1) % ENDPOINTS.size();
    sendQueryToEndpoint(query, nextEndpoint);
    return;
  }

  saveCache(query, responseBody);

  reply->deleteLater();
  emit requestSucceeded(response);
}
