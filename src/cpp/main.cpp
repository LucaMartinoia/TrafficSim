#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "backend.h"

static Backend* backendInstance = nullptr;
static QtMessageHandler previousHandler = nullptr;

// Handler to forward Qt IOstreams directly to QML console
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
  if (backendInstance)
  {
    QString severity;

    switch (type)
    {
    case QtDebugMsg:
      severity = "DEBUG";
      break;
    case QtInfoMsg:
      severity = "INFO";
      break;
    case QtWarningMsg:
      severity = "WARNING";
      break;
    case QtCriticalMsg:
      severity = "ERROR";
      break;
    case QtFatalMsg:
      severity = "FATAL";
      break;
    }

    QMetaObject::invokeMethod(backendInstance, "publishLogMessage", Qt::AutoConnection,
                              Q_ARG(QString, severity), Q_ARG(QString, message));
  }

  if (previousHandler)
    previousHandler(type, context, message);
}

int main(int argc, char* argv[])
{
  // Set QML-item style
  QQuickStyle::setStyle("Basic");

  QGuiApplication app(argc, argv);
  QCoreApplication::setOrganizationName(QStringLiteral("TrafficSim"));
  QCoreApplication::setApplicationName(QStringLiteral("LucaMartinoia"));

  // Register the backend to the QML engine
  QQmlApplicationEngine engine;
  Backend backend;

  // Register backend as endpoint for message forwarding
  backendInstance = &backend;
  previousHandler = qInstallMessageHandler(messageHandler);

  // Pass the backend as a required root property before loading the QML module
  engine.setInitialProperties(
      {{QStringLiteral("backend"), QVariant::fromValue(static_cast<QObject*>(&backend))}});
  engine.loadFromModule("TrafficSim", "Main");
  if (engine.rootObjects().isEmpty())
    return -1;

  qInfo() << ">>>\tStarting TrafficSim\t<<<\n";

  // Entry point
  return app.exec();
}