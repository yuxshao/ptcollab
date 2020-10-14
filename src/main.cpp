#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QSettings>
#include <QStyleFactory>

#include "editor/EditorWindow.h"
#include "editor/Settings.h"
#include "network/BroadcastServer.h"

const static QString stylesheet =
    "SideMenu QLabel, QTabWidget > QWidget { font-weight:bold; }"
    "QLineEdit { background-color: #00003e; color: #00F080; font-weight: bold; "
    "}"
    "QLineEdit:disabled { background-color: #343255; color: #9D9784; }"
    "QPushButton:disabled { color: #9D9784; }";

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // For QSettings
  a.setOrganizationName("ptcollab");
  a.setOrganizationDomain("ptweb.me");
  a.setApplicationName("pxtone collab");

  if (QSettings().value(CUSTOM_STYLE_KEY, true).toBool()) {
    a.setStyle(QStyleFactory::create("Fusion"));
    QPalette palette = qApp->palette();
    QColor text(222, 217, 187);
    QColor base(78, 75, 97);
    QColor button = base;
    QColor highlight(157, 151, 132);
    palette.setBrush(QPalette::Window, base);
    palette.setColor(QPalette::WindowText, text);
    palette.setBrush(QPalette::Base, base);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setBrush(QPalette::Button, base);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::Light, button.lighter(120));
    palette.setColor(QPalette::Dark, button.darker(120));
    qApp->setPalette(palette);
    qApp->setStyleSheet(stylesheet);
  }

  a.setApplicationVersion(Version::string());
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

  QCommandLineOption serverRecordOption(
      QStringList() << "r"
                    << "record",
      QCoreApplication::translate("main", "Record the session to this file."),
      QCoreApplication::translate("main", "record"));
  parser.addOption(serverRecordOption);

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
      QString recording_file = parser.value(serverRecordOption);
      BroadcastServer s(
          (filename == "" ? std::nullopt : std::optional(filename)), port,
          (recording_file == "" ? std::nullopt
                                : std::optional(recording_file)));

      return a.exec();
    }
  } else {
    EditorWindow w;
    w.show();
    return a.exec();
  }
}
