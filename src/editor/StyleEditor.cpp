#include "StyleEditor.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFile>
#include <QFontDatabase>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QUrl>
#include <set>

#include "Settings.h"

namespace StyleEditor {
const static char *SYSTEM_STYLE = "<System>";
static QString currentBasedir;
static QString currentStyleName;

QString styleSheetDir(const QString &basedir, const QString &styleName) {
  return basedir + "/" + styleName;
}
QString styleSheetPath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/" + styleName + ".qss";
}
struct InvalidColorError {
  QString settingsKey;
  QString setting;
};

inline bool processColorString(QColor *color, const QString str) {
  QString string = str;
  if (string.at(0) != "#") string.prepend("#");
  if (string.length() == 9) {
    QString rgb = string.chopped(2);
    if (QColor::isValidColor(rgb)) {
      color->setNamedColor(rgb);  // = new QColor(rgb);
      color->setAlpha(string.left(2).toInt());
      qDebug() << color->name();
      return 0;
    }

  } else if (string.length() == 7 && QColor::isValidColor(string)) {
    color->setNamedColor(string);  //  // new QColor(string);
    qDebug() << color->name();
    return 0;
  }
  return 1;
}

void setColorFromSetting(QPalette &palette, QPalette::ColorRole role,
                         QSettings &settings, const QString &key) {
  QString str = settings.value(key).toString();
  QColor *color = new QColor(Qt::magenta);
  if (!processColorString(color, str)) {
    palette.setColor(role, *color);
  } else
    throw InvalidColorError{key, str};
};

QColor getColorFromSetting(QSettings &settings, const QString &key,
                           QHash<QString, QColor> &fallback) {
  QString str = settings.value(key).toString();
  QColor *color = new QColor(Qt::magenta);
  if (!processColorString(color, str))
    return *color;
  else
    return fallback.find(key).value();
}

void tryLoadGlobalPalette(const QString &basedir, const QString &styleName) {
  QString path = styleSheetDir(basedir, styleName) + "/palette.ini";
  if (QFile::exists(path)) {
    QPalette palette = qApp->palette();
    QSettings stylePalette(path, QSettings::IniFormat);

    stylePalette.beginGroup("palette");
    try {
      setColorFromSetting(palette, QPalette::Window, stylePalette, "Window");
      setColorFromSetting(palette, QPalette::WindowText, stylePalette,
                          "WindowText");
      setColorFromSetting(palette, QPalette::Base, stylePalette, "Base");
      setColorFromSetting(palette, QPalette::ToolTipBase, stylePalette,
                          "ToolTipBase");
      setColorFromSetting(palette, QPalette::ToolTipText, stylePalette,
                          "ToolTipText");
      setColorFromSetting(palette, QPalette::Text, stylePalette, "Text");
      setColorFromSetting(palette, QPalette::Button, stylePalette, "Button");
      setColorFromSetting(palette, QPalette::ButtonText, stylePalette,
                          "ButtonText");
      setColorFromSetting(palette, QPalette::BrightText, stylePalette,
                          "BrightText");
      setColorFromSetting(palette, QPalette::Text, stylePalette, "Text");
      setColorFromSetting(palette, QPalette::Link, stylePalette, "Link");
      setColorFromSetting(palette, QPalette::Highlight, stylePalette,
                          "Highlight");
      setColorFromSetting(palette, QPalette::Light, stylePalette, "Light");
      setColorFromSetting(palette, QPalette::Dark, stylePalette, "Dark");
      qApp->setPalette(palette);
    } catch (InvalidColorError e) {
      qWarning() << QString(
                        "Could not load palette. Invalid color (%1) for "
                        "setting (%2)")
                        .arg(e.setting, e.settingsKey);
    }
    stylePalette.endGroup();
  }
}

QHash<QString, QColor> tryLoadMeterPalette() {
  QString path =
      styleSheetDir(currentBasedir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallbackMeterPalette;

  const QString BG_COLOR = "Background", BGCOLOR_SOFT = "BackgroundSoft",
                BAR_COLOR = "Bar", BAR_MID_COLOR = "BarMid",
                LABEL_COLOR = "Label", TICK_COLOR = "Tick",
                BAR_HIGH_COLOR = "BarHigh";

  //  fallbackMeterPalette.insert(BG_COLOR, QColor::fromRgb(26, 25, 73));
  //  fallbackMeterPalette.insert(BGCOLOR_SOFT, QColor::fromRgb(26, 25, 73,
  //  30)); fallbackMeterPalette.insert(BAR_COLOR, QColor::fromRgb(0, 240,
  //  128)); fallbackMeterPalette.insert(BAR_MID_COLOR, QColor::fromRgb(255,
  //  255, 128)); fallbackMeterPalette.insert(LABEL_COLOR, QColor::fromRgb(210,
  //  202, 156)); fallbackMeterPalette.insert(TICK_COLOR, QColor::fromRgb(52,
  //  50, 65)); fallbackMeterPalette.insert(BAR_HIGH_COLOR, Qt::red);

  // defaults -- acts as a fallback only if palette.ini doesn't have the
  // colors. we only have to do this for the meter & keyboard because they
  // don't have defaults like the QApplication palette does

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);

    stylePalette.beginGroup("meter");

    colors.insert(BG_COLOR, getColorFromSetting(stylePalette, BG_COLOR,
                                                fallbackMeterPalette));
    colors.insert(BGCOLOR_SOFT, getColorFromSetting(stylePalette, BGCOLOR_SOFT,
                                                    fallbackMeterPalette));
    colors.insert(BAR_COLOR, getColorFromSetting(stylePalette, BAR_COLOR,
                                                 fallbackMeterPalette));
    colors.insert(
        BAR_MID_COLOR,
        getColorFromSetting(stylePalette, BAR_MID_COLOR, fallbackMeterPalette));
    colors.insert(LABEL_COLOR, getColorFromSetting(stylePalette, LABEL_COLOR,
                                                   fallbackMeterPalette));
    colors.insert(TICK_COLOR, getColorFromSetting(stylePalette, TICK_COLOR,
                                                  fallbackMeterPalette));
    colors.insert(BAR_HIGH_COLOR,
                  getColorFromSetting(stylePalette, BAR_HIGH_COLOR,
                                      fallbackMeterPalette));
    stylePalette.endGroup();
  }
  return colors;
}
QHash<QString, QColor> tryLoadKeyboardPalette() {
  QString path =
      styleSheetDir(currentBasedir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallbackKeyboardPalette;

  const QString BEAT_COLOR = "Beat", ROOT_NOTE_COLOR = "RootNote",
                WHITE_NOTE_COLOR = "WhiteNote", BLACK_NOTE_COLOR = "BlackNote",
                WHITE_LEFT_COLOR = "WhiteLeft", BLACK_LEFT_COLOR = "BlackLeft",
                BLACK_COLOR = "Black";

  fallbackKeyboardPalette.insert(BEAT_COLOR, QColor::fromRgb(128, 128, 128));
  fallbackKeyboardPalette.insert(ROOT_NOTE_COLOR, QColor::fromRgb(84, 76, 76));
  fallbackKeyboardPalette.insert(WHITE_NOTE_COLOR, QColor::fromRgb(64, 64, 64));
  fallbackKeyboardPalette.insert(BLACK_NOTE_COLOR, QColor::fromRgb(32, 32, 32));
  fallbackKeyboardPalette.insert(WHITE_LEFT_COLOR,
                                 QColor::fromRgb(131, 126, 120, 128));
  fallbackKeyboardPalette.insert(BLACK_LEFT_COLOR,
                                 QColor::fromRgb(78, 75, 97, 128));
  fallbackKeyboardPalette.insert(BLACK_COLOR, Qt::black);

  // defaults -- acts as a fallback only if palette.ini doesn't have the
  // colors. we only have to do this for the meter & keyboard because they
  // don't have defaults like the QApplication palette does

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);

    stylePalette.beginGroup("keyboard");

    colors.insert(BEAT_COLOR, getColorFromSetting(stylePalette, BEAT_COLOR,
                                                  fallbackKeyboardPalette));
    colors.insert(ROOT_NOTE_COLOR,
                  getColorFromSetting(stylePalette, ROOT_NOTE_COLOR,
                                      fallbackKeyboardPalette));
    colors.insert(WHITE_NOTE_COLOR,
                  getColorFromSetting(stylePalette, WHITE_NOTE_COLOR,
                                      fallbackKeyboardPalette));
    colors.insert(BLACK_NOTE_COLOR,
                  getColorFromSetting(stylePalette, BLACK_NOTE_COLOR,
                                      fallbackKeyboardPalette));
    colors.insert(WHITE_LEFT_COLOR,
                  getColorFromSetting(stylePalette, WHITE_LEFT_COLOR,
                                      fallbackKeyboardPalette));
    colors.insert(BLACK_LEFT_COLOR,
                  getColorFromSetting(stylePalette, BLACK_LEFT_COLOR,
                                      fallbackKeyboardPalette));
    colors.insert(BLACK_COLOR, getColorFromSetting(stylePalette, BLACK_COLOR,
                                                   fallbackKeyboardPalette));
    stylePalette.endGroup();
  }
  return colors;
}

void initializeStyleDir() {
  QString optimalLocation =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/styles/";

  QFile readme(":/styles/README.md");
  QDir outputDir(optimalLocation);
  if (!outputDir.exists(optimalLocation)) {
    outputDir.mkpath(optimalLocation);
    readme.copy(optimalLocation + "README.md");
  }
  QDesktopServices::openUrl(optimalLocation);
}
QString relativizeUrls(QString stylesheet, const QString &basedir,
                       const QString &styleName) {
  stylesheet.replace("url(\"",
                     "url(\"" + styleSheetDir(basedir, styleName) + "/");
  return stylesheet;
}

void loadFonts(const QString path) {
  QDirIterator it(path, QDir::NoDotAndDotDot | QDir::Files,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    QFileInfo e(it.filePath());
    if (e.suffix() == "otf" || e.suffix() == "ttf") {
      qDebug() << "Found font at" << it.filePath();
      QFontDatabase::addApplicationFont(it.filePath());
    }
  }
}
bool tryLoadStyle(const QString &basedir, const QString &styleName) {
  QFile styleSheet = styleSheetPath(basedir, styleName);
  if (!styleSheet.open(QFile::ReadOnly)) {
    qWarning() << "The selected style is not available: " << styleName << " ("
               << styleSheet.fileName() << ")";
    return false;
  }

  currentStyleName = styleName;
  currentBasedir = basedir;
  tryLoadGlobalPalette(basedir, styleName);
  loadFonts(styleSheetDir(basedir, styleName));
  // Only apply custom palette if palette.ini is present. For minimal
  // sheets, the availability of a palette helps their changes blend
  // in with unchanged aspects.
  qApp->setStyle(QStyleFactory::create("Fusion"));
  // Use Fusion as a base for aspects stylesheet does not cover, it should
  // look consistent across all platforms
  qApp->setStyleSheet(relativizeUrls(styleSheet.readAll(), basedir, styleName));
  styleSheet.close();
  return true;
}

std::map<QString, QString> getStyleMap() {
  std::map<QString, QString> styles;
  styles[SYSTEM_STYLE] = "";

  QString exe_path = qApp->applicationDirPath();
  QStringList dirsToCheck =
      // Prioritize things in user config dir over application dir
      QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation) +
      QStringList{exe_path, exe_path + "/../share", ":"};

  for (QString basedir : dirsToCheck) {
    basedir += "/styles";
    qDebug() << "Searching for styles in: " << basedir;
    QDirIterator dir(basedir,
                     QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                     QDirIterator::NoIteratorFlags);
    while (dir.hasNext()) {
      dir.next();

      QString styleName = dir.fileName();
      if (styles.count(styleName) > 0) continue;

      QString stylePath = styleSheetPath(basedir, styleName);
      if (!QFile(stylePath).exists()) continue;
      qDebug() << "Found style" << styleName << "at path" << stylePath;
      styles[styleName] = basedir;
    }
  }
  return styles;
}

bool tryLoadStyle(const QString &styleName) {
  if (styleName == SYSTEM_STYLE) return true;

  auto styles = getStyleMap();
  auto it = styles.find(styleName);

  if (it == styles.end()) {
    qWarning() << "No such available style: " << styleName;
    return false;
  }

  QString basedir = it->second;
  currentStyleName = styleName;
  currentBasedir = basedir;
  return tryLoadStyle(basedir, styleName);
}

QStringList getStyles() {
  QStringList styles;
  for (const auto &[style, dir] : getStyleMap()) styles.push_back(style);
  return styles;
}
}  // namespace StyleEditor
