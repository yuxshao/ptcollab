#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QStyleFactory>
#include "editor/EditorWindow.h"
#include "network/BroadcastServer.h"

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // For QSettings
  a.setOrganizationName("ptcollab");
  a.setOrganizationDomain("ptweb.me");
  a.setApplicationName("pxtone collab");
  if (QFileInfo("./fusion.style").exists()) QApplication::setStyle(QStyleFactory::create("Fusion"));
 /* QPalette palette = qApp->palette();
  palette.setColor(QPalette::Window, QColor(78, 75, 97));
  palette.setColor(QPalette::WindowText, Qt::white);
  palette.setColor(QPalette::Base, QColor(25, 25, 25));
  palette.setColor(QPalette::AlternateBase, QColor(52, 50, 85));
  palette.setColor(QPalette::ToolTipBase, Qt::white);
  palette.setColor(QPalette::ToolTipText, Qt::white);
  palette.setColor(QPalette::Text, Qt::white);
  palette.setColor(QPalette::Button, QColor(78, 75, 97));
  palette.setColor(QPalette::ButtonText, Qt::white);
  palette.setColor(QPalette::BrightText, Qt::red);
  palette.setColor(QPalette::Link, QColor(157, 151, 132));
  palette.setColor(QPalette::Highlight, QColor(157, 151, 132));
  palette.setColor(QPalette::HighlightedText, Qt::black);
  qApp->setPalette(palette);*/

  a.setApplicationVersion(QString(GIT_VERSION));
  QCommandLineParser parser;
  parser.setApplicationDescription("A collaborative pxtone editor");

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
