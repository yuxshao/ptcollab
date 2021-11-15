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
const char *SYSTEM_STYLE = "<System>";

QString styleSheetDir(const QString &basedir, const QString &styleName) {
  return basedir + "/" + styleName;
}
QString styleSheetPath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/" + styleName + ".qss";
}
QString palettePath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/palette.ini";
}

struct InvalidColorError {
  QString settingsKey;
  QString setting;
};

void withSettingsColor(const QSettings &settings, const QString &key,
                       std::function<void(const QColor &)> f) {
  if (settings.contains(key)) {
    QString str = settings.value(key).toString();
    if (QColor::isValidColor(str))
      f(QColor(str));
    else
      qWarning() << QString(
                        "Invalid color (%1) for "
                        "setting (%2) in palette")
                        .arg(str, key);
  }
}

void setQPaletteColor(QPalette &palette, QPalette::ColorRole role,
                      QSettings &settings, const QString &key) {
  withSettingsColor(settings, key,
                    [&](const QColor &c) { palette.setColor(role, c); });
};

void loadQPalette(const QString &path, QPalette &palette) {
  QSettings stylePalette(path, QSettings::IniFormat);

  stylePalette.beginGroup("palette");
  setQPaletteColor(palette, QPalette::Window, stylePalette, "Window");
  setQPaletteColor(palette, QPalette::WindowText, stylePalette, "WindowText");
  setQPaletteColor(palette, QPalette::Base, stylePalette, "Base");
  setQPaletteColor(palette, QPalette::ToolTipBase, stylePalette, "ToolTipBase");
  setQPaletteColor(palette, QPalette::ToolTipText, stylePalette, "ToolTipText");
  setQPaletteColor(palette, QPalette::Text, stylePalette, "Text");
  setQPaletteColor(palette, QPalette::Button, stylePalette, "Button");
  setQPaletteColor(palette, QPalette::ButtonText, stylePalette, "ButtonText");
  setQPaletteColor(palette, QPalette::BrightText, stylePalette, "BrightText");
  setQPaletteColor(palette, QPalette::Text, stylePalette, "Text");
  setQPaletteColor(palette, QPalette::Link, stylePalette, "Link");
  setQPaletteColor(palette, QPalette::Highlight, stylePalette, "Highlight");
  setQPaletteColor(palette, QPalette::Light, stylePalette, "Light");
  setQPaletteColor(palette, QPalette::Dark, stylePalette, "Dark");
  stylePalette.endGroup();
}

void setPaletteColor(const QSettings &settings, QColor &dst,
                     const QString &key) {
  withSettingsColor(settings, key, [&](const QColor &c) { dst = c; });
};

void loadPalette(const QString &path, Palette &p) {
  QSettings stylePalette(path, QSettings::IniFormat);
  setPaletteColor(stylePalette, p.MeterBackground, "meter/Background");
  setPaletteColor(stylePalette, p.MeterBackgroundSoft, "meter/BackgroundSoft");
  setPaletteColor(stylePalette, p.MeterBar, "meter/Bar");
  setPaletteColor(stylePalette, p.MeterBarMid, "meter/BarMid");
  setPaletteColor(stylePalette, p.MeterLabel, "meter/Label");
  setPaletteColor(stylePalette, p.MeterTick, "meter/Tick");
  setPaletteColor(stylePalette, p.MeterBarHigh, "meter/BarHigh");

  setPaletteColor(stylePalette, p.KeyboardBeat, "keyboard/Beat");
  setPaletteColor(stylePalette, p.KeyboardRootNote, "keyboard/RootNote");
  setPaletteColor(stylePalette, p.KeyboardWhiteNote, "keyboard/WhiteNote");
  setPaletteColor(stylePalette, p.KeyboardBlackNote, "keyboard/BlackNote");
  setPaletteColor(stylePalette, p.KeyboardWhiteLeft, "keyboard/WhiteLeft");
  setPaletteColor(stylePalette, p.KeyboardBlackLeft, "keyboard/BlackLeft");
  setPaletteColor(stylePalette, p.KeyboardBlack, "keyboard/Black");
  setPaletteColor(stylePalette, p.KeyboardMeasure, "keyboard/Measure");

  setPaletteColor(stylePalette, p.MeasureSeparator, "measure/MeasureSeparator");
  setPaletteColor(stylePalette, p.MeasureIncluded, "measure/MeasureIncluded");
  setPaletteColor(stylePalette, p.MeasureExcluded, "measure/MeasureExcluded");
  setPaletteColor(stylePalette, p.MeasureBeat, "measure/Beat");
  setPaletteColor(stylePalette, p.MeasureUnitEdit, "measure/UnitEdit");
  setPaletteColor(stylePalette, p.MeasureNumberBlock,
                  "measure/MeasureNumberBlock");

  setPaletteColor(stylePalette, p.ParamBlue, "parameters/Blue");
  setPaletteColor(stylePalette, p.ParamDarkBlue, "parameters/DarkBlue");
  setPaletteColor(stylePalette, p.ParamDarkTeal, "parameters/DarkTeal");
  setPaletteColor(stylePalette, p.ParamBrightGreen, "parameters/BrightGreen");
  setPaletteColor(stylePalette, p.ParamFadedWhite, "parameters/FadedWhite");
  setPaletteColor(stylePalette, p.ParamFont, "parameters/Font");
  setPaletteColor(stylePalette, p.ParamBeat, "parameters/Beat");
  setPaletteColor(stylePalette, p.ParamMeasure, "parameters/Measure");

  setPaletteColor(stylePalette, p.Playhead, "views/Playhead");
  setPaletteColor(stylePalette, p.Cursor, "views/Cursor");
}

Palette defaultPalette(bool is_system_theme) {
  Palette p = Palette::empty();
  loadPalette(":/styles/ptCollage/palette.ini", p);

  if (is_system_theme) {
    // Use a slightly different colour scheme since the ptcollage style is a bit
    // jarring
    p.MeterBackground = qApp->palette().dark().color();
    p.MeterBackgroundSoft = qApp->palette().dark().color();
    p.MeterBar = Qt::green;
    p.MeterBarMid = Qt::yellow;
    p.MeterLabel = qApp->palette().text().color();
    p.MeterTick = qApp->palette().shadow().color();
    p.MeterBarHigh = Qt::red;
  }

  return p;
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

static std::shared_ptr<QPixmap> currentMeasureImages = nullptr;
const std::shared_ptr<QPixmap> measureImages() { return currentMeasureImages; }

static Palette currentPalette = Palette::empty();
const Palette &palette = currentPalette;

bool tryLoadStyle(const QString &basedir, const QString &styleName) {
  // A stylesheet needs to exist for any part of the style to be loaded.
  QFile styleSheet = styleSheetPath(basedir, styleName);
  if (!styleSheet.open(QFile::ReadOnly)) {
    qWarning() << "The selected style is not available: " << styleName << " ("
               << styleSheet.fileName() << ")";
    return false;
  }

  QString path = palettePath(basedir, styleName);
  if (QFile::exists(path)) {
    QPalette qp(qApp->palette());
    loadQPalette(path, qp);
    qApp->setPalette(qp);

    Palette p = defaultPalette(false);
    loadPalette(path, p);
    currentPalette = p;
  }

  loadFonts(styleSheetDir(basedir, styleName));
  qApp->setStyle(QStyleFactory::create("Fusion"));
  // Use Fusion as a base for aspects stylesheet does not cover, it should
  // look consistent across all platforms
  qApp->setStyleSheet(relativizeUrls(styleSheet.readAll(), basedir, styleName));
  styleSheet.close();

  currentMeasureImages = std::make_shared<QPixmap>(":/images/images");
  QPixmap px(styleSheetDir(basedir, styleName) + "/images.png");
  if (!px.isNull() && px.size() == currentMeasureImages->size())
    currentMeasureImages = std::make_shared<QPixmap>(px);
  //  else
  //    currentMeasureImages = std::make_shared<QPixmap>(":/images/images");
  //
  // Now redundant because of the way currentMeasureImages is defined
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
  if (styleName == SYSTEM_STYLE) {
    currentPalette = defaultPalette(true);
    currentMeasureImages = std::make_shared<QPixmap>(":/images/images");
    return true;
  }

  auto styles = getStyleMap();
  auto it = styles.find(styleName);

  if (it == styles.end()) {
    qWarning() << "No such available style: " << styleName;
    return false;
  }

  QString basedir = it->second;
  return tryLoadStyle(basedir, styleName);
}

QStringList getStyles() {
  QStringList styles;
  for (const auto &[style, dir] : getStyleMap()) styles.push_back(style);
  return styles;
}
}  // namespace StyleEditor
