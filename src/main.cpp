#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QStyleFactory>

#include "editor/EditorWindow.h"
#include "editor/Settings.h"
#include "network/BroadcastServer.h"

const QString requestedDefaultStyleName =
    "ptCollage";  // This is the desired default style. In the event this style
                  // is problematic, which is entirely possible, it will not be
                  // re-set as the style; the System theme then will be used.
                  // I don't see the need to have this change after
                  // compile-time. The purpose of these styles is to be
                  // configurable, and the user's preference is respected given
                  // their respected style is not problematic. The name of this
                  // style should be consistent, reliable, and guaranteed on a
                  // fresh instance (installation or compilation). And if not,
                  // there won't be any errors.

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

void determineStyle() {
  QString styleName = Settings::StyleName::get();
  if (styleName.isEmpty() ||
      !QFile::exists(qApp->applicationDirPath() + "/style/" + styleName + "/" +
                     styleName + ".qss")) {
    QFile requestedDefaultStyle = qApp->applicationDirPath() + "/style/" +
                                  requestedDefaultStyleName + "/" +
                                  requestedDefaultStyleName + ".qss";
    if (requestedDefaultStyle.exists()) {
      Settings::StyleName::set(requestedDefaultStyleName);
    } else {
      Settings::StyleName::set("System");
    }
    return;
  }
  // Set default style if current style is not
  // present or unset (meaning files were moved between now and the last start
  // or settings are unset/previously cleared). In that case set default style
  // to ptCollage in every instance possible unless erreneous or absent (then
  // it's "System").
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  // For QSettings
  a.setOrganizationName("ptcollab");
  a.setOrganizationDomain("ptweb.me");
  a.setApplicationName("pxtone collab");

  determineStyle();

  QString styleSheetName = Settings::StyleName::get();
  QFile styleSheet = qApp->applicationDirPath() + "/style/" + styleSheetName +
                     "/" + styleSheetName + ".qss";
  styleSheet.open(QFile::ReadOnly);

  if (Settings::StyleName::get() == "System") {
    Settings::StyleName::set(requestedDefaultStyleName);
  } else {
    if (styleSheet.isReadable()) {
      if (QFile::exists(qApp->applicationDirPath() + "/style/" +
                        styleSheetName + "/palette.ini")) {
        QPalette palette = qApp->palette();
        QSettings stylePalette(qApp->applicationDirPath() + "/style/" +
                                   styleSheetName + "/palette.ini",
                               QSettings::IniFormat);

        stylePalette.beginGroup("palette");
        palette.setColor(QPalette::Window,
                         stylePalette.value("Window").toString());
        palette.setColor(QPalette::WindowText,
                         stylePalette.value("WindowText").toString());
        palette.setColor(QPalette::Base, stylePalette.value("Base").toString());
        palette.setColor(QPalette::ToolTipBase,
                         stylePalette.value("ToolTipBase").toString());
        palette.setColor(QPalette::ToolTipText,
                         stylePalette.value("ToolTipText").toString());
        palette.setColor(QPalette::Text, stylePalette.value("Text").toString());
        palette.setColor(QPalette::Button,
                         stylePalette.value("Button").toString());
        palette.setColor(QPalette::ButtonText,
                         stylePalette.value("ButtonText").toString());
        palette.setColor(QPalette::BrightText,
                         stylePalette.value("BrightText").toString());
        palette.setColor(QPalette::Text, stylePalette.value("Text").toString());
        palette.setColor(QPalette::Link, stylePalette.value("Link").toString());
        palette.setColor(QPalette::Highlight,
                         stylePalette.value("Highlight").toString());
        qApp->setPalette(palette);
      }
      qApp->setStyle(QStyleFactory::create("Fusion"));
      qApp->setStyleSheet(styleSheet.readAll());
      // Use Fusion as a base for aspects stylesheet does not cover, it should
      // look consistent across all platforms
    } else {
      QMessageBox::critical(nullptr, "Style Error",
                            "The selected style (" +
                                Settings::StyleName::get() +
                                ") has errors. The "
                                "default style will be used.");
      if (Settings::StyleName::get() == requestedDefaultStyleName)
        Settings::StyleName::set("System");
      // This should only happen if there are unforseen platform/filesystem/Qt
      // errors or tinkering was done with the ptCollage style
    }
    // Only apply custom palette if palette.ini is present. QPalette::Light
    // and QPalette::Dark have been discarded because they are unlikely to be
    // used in conjunction with stylesheets & they cannot be represented
    // properly in an .INI. There are situations where the palette would not be
    // a necessary addition to the stylesheet & all the coloring is done
    // first-hand by the stylesheet author. For minimal sheets, though, the
    // availability of a palette helps their changes blend in with unchanged
    // aspects.
  }

  styleSheet.close();

  a.setApplicationVersion(Settings::Version::string());
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
    return a.exec();
  } else {
    EditorWindow w;
    w.show();
    if (startServerImmediately)
      w.hostDirectly(filename, host, port, recording_file, username);
    return a.exec();
  }
}
