#include <QApplication>
#include <QCommandLineParser>
#include <QFile>

#include "editor/EditorWindow.h"
#include "server/BroadcastServer.h"

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  a.setApplicationVersion(QString(GIT_VERSION));
  QCommandLineParser parser;
  parser.setApplicationDescription("pxtone collab");

  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption serverPortOption(
      QStringList() << "p"
                    << "port",
      QCoreApplication::translate("main", "Just run a server on port <port>."),
      QCoreApplication::translate("main", "port"));

  parser.addOption(serverPortOption);
  QCommandLineOption serverFileOption(
      QStringList() << "f"
                    << "file",
      QCoreApplication::translate("main",
                                  "Load this file when starting the server."),
      QCoreApplication::translate("main", "file"));

  parser.addOption(serverFileOption);

  parser.process(a);
  QString portStr = parser.value(serverPortOption);
  if (portStr != "") {
    bool ok;
    int port = portStr.toInt(&ok);
    if (!ok)
      qFatal("Could not parse port");
    else {
      qInfo() << "Running on port" << port;
      QString filename = parser.value(serverFileOption);
      QByteArray data;
      if (filename != "") {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) qFatal("Could not read file.");
        data = file.readAll();
      }
      BroadcastServer s(std::move(data), port);

      return a.exec();
    }
  } else {
    EditorWindow w;
    w.show();
    return a.exec();
  }
}
