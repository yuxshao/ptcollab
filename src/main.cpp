#include <QApplication>
#include <QCommandLineParser>

#include "editor/EditorWindow.h"
#include "editor/Settings.h"
#include "editor/StyleEditor.h"
#include "network/BroadcastServer.h"

static FILE *logDestination = stderr;
void messageHandler(QtMsgType type, const QMessageLogContext &context,
                    const QString &msg) {
  QByteArray localMsg = msg.toLocal8Bit();
  const char *file = context.file ? context.file : "";
  const char *function = context.function ? context.function : "";
  std::string now_str =
      QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toStdString();
  const char *now = now_str.c_str();
  switch (type) {
    case QtDebugMsg:
      fprintf(logDestination, "[%s] Debug: %s (%s:%u, %s)\n", now,
              localMsg.constData(), file, context.line, function);
      break;
    case QtInfoMsg:
      fprintf(logDestination, "[%s] Info: %s (%s:%u, %s)\n", now,
              localMsg.constData(), file, context.line, function);
      break;
    case QtWarningMsg:
      fprintf(logDestination, "[%s] Warning: %s (%s:%u, %s)\n", now,
              localMsg.constData(), file, context.line, function);
      break;
    case QtCriticalMsg:
      fprintf(logDestination, "[%s] Critical: %s (%s:%u, %s)\n", now,
              localMsg.constData(), file, context.line, function);
      break;
    case QtFatalMsg:
      fprintf(logDestination, "[%s] Fatal: %s (%s:%u, %s)\n", now,
              localMsg.constData(), file, context.line, function);
      break;
  }
  fflush(logDestination);
}

QCoreApplication *createApplication(int &argc, char *argv[]) {
  QStringList args(argv, argv + argc);
  if (args.contains("-headless") || args.contains("--headless"))
    return new QCoreApplication(argc, argv);
  return new QApplication(argc, argv);
}

int main(int argc, char *argv[]) {
  QScopedPointer<QCoreApplication> a(createApplication(argc, argv));
  a->setOrganizationName("ptcollab");
  a->setOrganizationDomain("ptweb.me");
  a->setApplicationName("ptcollab");
  a->setApplicationVersion(Settings::Version::string());

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

  QCommandLineOption logFileOption(
      QStringList() << "log",
      QCoreApplication::translate("main", "Write log messages to this file."),
      QCoreApplication::translate("main", "file"));
  parser.addOption(logFileOption);

  QCommandLineOption clearSettingsOption(
      "clear-settings",
      QCoreApplication::translate("main", "Clear application settings."));
  parser.addOption(clearSettingsOption);
  parser.process(*a);

  if (parser.isSet(clearSettingsOption)) {
    Settings::clear();
    qWarning("Settings have been cleared.");
    return 0;
  }

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
    Settings::setValue(DISPLAY_NAME_KEY, username);
  username = Settings::value(DISPLAY_NAME_KEY, "").toString();

  QString logFile = parser.value(logFileOption);
  if (logFile != "") {
    FILE *file = fopen(logFile.toStdString().c_str(), "a");
    if (file == nullptr) {
      std::cerr << "Cannot open file" << logFile.toStdString()
                << "for logging.\n";
      return 1;
    } else
      logDestination = file;
  }
  qInstallMessageHandler(messageHandler);

  if (parser.isSet(headlessOption)) {
    BroadcastServer s(filename, host, port, recording_file);
    return a->exec();
  } else {
    QString style = Settings::StyleName::get();
    if (!StyleEditor::tryLoadStyle(style)) {
      QString defaultStyle =
          Settings::StyleName::default_included_with_distribution;
      if (style == defaultStyle || !StyleEditor::tryLoadStyle(defaultStyle))
        qWarning() << "No styles were loaded. Falling back on system style.";
    }
    EditorWindow w;
    a->installEventFilter(&w);
    w.show();
    if (startServerImmediately)
      w.hostDirectly(filename, host, port, recording_file, username);
    return a->exec();
  }
}
