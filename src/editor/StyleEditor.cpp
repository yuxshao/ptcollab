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
static QString currentStyleBaseDir;
static QString currentStyleName = SYSTEM_STYLE;

void setSystemStyle() {
  currentStyleName = SYSTEM_STYLE;
  // other things may be necessary in the future
}

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

const QPixmap getMeasureImages() {
  QPixmap px(styleSheetDir(currentStyleBaseDir, currentStyleName) +
             "/images.png");
  if (!px.isNull() && px.width() == 108 && px.height() == 65) {
    return px;  // the rare case in which an image has to be a fixed size
  } else
    return QPixmap(":/images/images");
}

inline bool processColorString(QColor *color, const QString str) {
  QString string = str;
  if (string.isEmpty()) return 1;
  if (string.at(0) != "#") string.prepend("#");
  switch (string.length()) {
    case 4: {  // e.g. #RGB
      QString rgb;
      rgb.append("#");
      rgb.append(string.at(1));
      rgb.append(string.at(1));
      rgb.append(string.at(2));
      rgb.append(string.at(2));
      rgb.append(string.at(3));
      rgb.append(string.at(3));  // help me find a better way to do this
      if (QColor::isValidColor(rgb)) {
        color->setNamedColor(rgb);
        return 0;
      }
      break;
    }
    case 7: {  // e.g. #RRGGBB
      if (QColor::isValidColor(string)) {
        color->setNamedColor(string);
        return 0;
      }
      break;
    }
    case 9: {  // e.g. #RRGGBBAA
      QString rgb = string.chopped(2);
      if (QColor::isValidColor(rgb)) {
        color->setNamedColor(rgb);
        color->setAlpha(string.right(2).toInt(nullptr, 16));
        return 0;
      }
      break;
    }
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

void constructList(QStringList list, QHash<QString, QColor> *hash,
                   QSettings *settings, QHash<QString, QColor> *fallback) {
  QStringListIterator it(list);
  while (it.hasNext()) {
    QString current = it.next();
    hash->insert(current, getColorFromSetting(*settings, current, *fallback));
  }
}

QHash<QString, QColor> getMeterPalette() {
  QString path =
      styleSheetDir(currentStyleBaseDir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallback;

  const QStringList colorList{"Background", "BackgroundSoft", "Bar",
                              "BarMid",     "Label",          "Tick",
                              "BarHigh"};

  if (currentStyleName == SYSTEM_STYLE) {
    fallback.insert(colorList.at(0), qApp->palette().dark().color());
    fallback.insert(colorList.at(1), qApp->palette().dark().color());
    fallback.insert(colorList.at(2), Qt::green);
    fallback.insert(colorList.at(3), Qt::yellow);
    fallback.insert(colorList.at(4), qApp->palette().text().color());
    fallback.insert(colorList.at(5), qApp->palette().shadow().color());
    fallback.insert(colorList.at(6), Qt::red);
  } else {
    fallback.insert(colorList.at(0),
                    QColor::fromRgb(26, 25, 73));  // Background
    fallback.insert(colorList.at(1),
                    QColor::fromRgb(26, 25, 73, 30));  // BackgroundSoft
    fallback.insert(colorList.at(2), QColor::fromRgb(0, 240, 128));    // Bar
    fallback.insert(colorList.at(3), QColor::fromRgb(255, 255, 128));  // BarMid
    fallback.insert(colorList.at(4), QColor::fromRgb(210, 202, 156));  // Label
    fallback.insert(colorList.at(5), QColor::fromRgb(52, 50, 65));     // Tick
    fallback.insert(colorList.at(6), Qt::red);  // BarHigh
  }

  if (currentStyleName == SYSTEM_STYLE) return fallback;

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);
    stylePalette.beginGroup("meter");
    constructList(colorList, &colors, &stylePalette, &fallback);
    stylePalette.endGroup();
  }
  return colors;
}
QHash<QString, QColor> getKeyboardPalette() {
  QString path =
      styleSheetDir(currentStyleBaseDir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallback;

  const QStringList colorList{"Beat",      "RootNote",  "WhiteNote",
                              "BlackNote", "WhiteLeft", "BlackLeft",
                              "Black"};

  fallback.insert(colorList.at(0), QColor::fromRgb(128, 128, 128));  // Beat
  fallback.insert(colorList.at(1), QColor::fromRgb(84, 76, 76));     // RootNote
  fallback.insert(colorList.at(2), QColor::fromRgb(64, 64, 64));  // WhiteNote
  fallback.insert(colorList.at(3), QColor::fromRgb(32, 32, 32));  // BlackNote
  fallback.insert(colorList.at(4),
                  QColor::fromRgb(131, 126, 120, 128));  // WhiteLeft
  fallback.insert(colorList.at(5),
                  QColor::fromRgb(78, 75, 97, 128));  // BlackLeft
  fallback.insert(colorList.at(6), Qt::black);        // Black

  if (currentStyleName == SYSTEM_STYLE) return fallback;

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);
    stylePalette.beginGroup("keyboard");
    constructList(colorList, &colors, &stylePalette, &fallback);
    stylePalette.endGroup();
  }
  return colors;
}

QHash<QString, QColor> getMeasurePalette() {
  QString path =
      styleSheetDir(currentStyleBaseDir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallback;

  const QStringList colorList{
      "Playhead",        "Cursor", "MeasureSeparator", "MeasureIncluded",
      "MeasureExcluded", "Beat",   "UnitEdit",         "MeasureNumberBlock"};

  fallback.insert(colorList.at(0), Qt::white);              // Playhead
  fallback.insert(colorList.at(1), Qt::white);              // Cursor
  fallback.insert(colorList.at(2), Qt::white);              // MeasureSeparator
  fallback.insert(colorList.at(3), QColor(128, 0, 0));      // MeasureIncluded
  fallback.insert(colorList.at(4), QColor(64, 0, 0));       // MeasureExcluded
  fallback.insert(colorList.at(5), QColor(128, 128, 128));  // Beat
  fallback.insert(colorList.at(6), QColor(64, 0, 112));     // UnitEdit
  fallback.insert(colorList.at(7), QColor(96, 96, 96));  // MeasureNumberBlock

  if (currentStyleName == SYSTEM_STYLE) return fallback;

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);
    stylePalette.beginGroup("measure");
    constructList(colorList, &colors, &stylePalette, &fallback);
    stylePalette.endGroup();
  }
  return colors;
}

QHash<QString, QColor> getParametersPalette() {
  QString path =
      styleSheetDir(currentStyleBaseDir, currentStyleName) + "/palette.ini";
  QHash<QString, QColor> colors;
  QHash<QString, QColor> fallback;

  const QStringList colorList{"Blue",        "DarkBlue",   "DarkTeal",
                              "BrightGreen", "FadedWhite", "Font",
                              "Beat",        "Measure"};

  fallback.insert(colorList.at(0), QColor(52, 50, 85));     // Blue
  fallback.insert(colorList.at(1), QColor(26, 25, 73));     // DarkBlue
  fallback.insert(colorList.at(2), QColor(0, 96, 96));      // DarkTeal
  fallback.insert(colorList.at(3), QColor(0, 240, 128));    // BrightGreen
  fallback.insert(colorList.at(4), Qt::white);              // FadedWhite
  fallback.insert(colorList.at(5), Qt::white);              // Font
  fallback.insert(colorList.at(6), QColor(128, 128, 128));  // Beat
  fallback.insert(colorList.at(7), Qt::white);              // Measure

  if (currentStyleName == SYSTEM_STYLE) return fallback;

  if (QFile::exists(path)) {
    QSettings stylePalette(path, QSettings::IniFormat);
    stylePalette.beginGroup("parameters");
    constructList(colorList, &colors, &stylePalette, &fallback);
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
  currentStyleBaseDir = basedir;
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
  currentStyleBaseDir = basedir;
  return tryLoadStyle(basedir, styleName);
}

QStringList getStyles() {
  QStringList styles;
  for (const auto &[style, dir] : getStyleMap()) styles.push_back(style);
  return styles;
}
}  // namespace StyleEditor
