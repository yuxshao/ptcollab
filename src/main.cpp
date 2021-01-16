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

  if (CustomStyle::get()) {
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
      QCoreApplication::translate("main", "Fix the server port to <port>."),
      QCoreApplication::translate("main", "port"));
  parser.addOption(serverPortOption);

  QCommandLineOption serverAddressOption(
      QStringList() << "l"
                    << "listen"
                    << "address",
      QCoreApplication::translate("main",
                                  "Listen on this address (use 127.0.0.1 for "
                                  "private, 0.0.0.0 for public)."),
      QCoreApplication::translate("main", "host"));
  parser.addOption(serverAddressOption);

  QCommandLineOption serverRecordOption(
      QStringList() << "r"
                    << "record",
      QCoreApplication::translate("main", "Record the session to this file."),
      QCoreApplication::translate("main", "record"));
  parser.addOption(serverRecordOption);

  QCommandLineOption headlessOption(
      QStringList() << "headless",
      QCoreApplication::translate("main", "Just run a server with no editor."));
  parser.addOption(headlessOption);

  parser.addPositionalArgument(
      "file",
      QCoreApplication::translate("main", "Load this file when starting."),
      "[file]");

  QCommandLineOption usernameOption(
      QStringList() << "u"
                    << "username",
      QCoreApplication::translate("main", "Join with this username."));
  parser.addOption(usernameOption);

  parser.process(a);

  bool startServerImmediately = false;
  std::optional<QString> filename = std::nullopt;
  if (parser.positionalArguments().length() > 1)
    qFatal("Too many positional arguments given.");
  if (parser.positionalArguments().length() == 1) {
    QString f = parser.positionalArguments().at(0);
    if (f != "") {
      filename = f;
      startServerImmediately = true;
    }
  }

  int port = 0;
  QString portStr = parser.value(serverPortOption);
  if (portStr != "") {
    bool ok;
    port = portStr.toInt(&ok);
    if (!ok) qFatal("Could not parse port");
    startServerImmediately = true;
  }

  QHostAddress host(QHostAddress::LocalHost);
  QString hostStr = parser.value(serverAddressOption);
  if (hostStr != "") {
    host = QHostAddress(hostStr);
    startServerImmediately = true;
  }

  std::optional<QString> recording_file = parser.value(serverRecordOption);
  if (recording_file != "")
    startServerImmediately = true;
  else
    recording_file = std::nullopt;

  QString username = parser.value(usernameOption);
  if (parser.value(usernameOption) != "")
    QSettings().setValue(DISPLAY_NAME_KEY, username);
  username = QSettings().value(DISPLAY_NAME_KEY).toString();

  if (parser.isSet(headlessOption)) {
    BroadcastServer s(filename, host, port, recording_file);
    return a.exec();
  } else {
    EditorWindow w;
    w.show();
    if (startServerImmediately)
      w.hostDirectly(filename, host, port, recording_file, username);
    return a.exec();
  }
}
