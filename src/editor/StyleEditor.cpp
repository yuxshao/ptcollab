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
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QUrl>
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#include <set>

namespace StyleEditor {
const char *SYSTEM_STYLE = "<System>";

QString styleSheetDir(const QString &basedir, const QString &styleName) {
  return basedir + "/" + styleName;
}
QString styleSheetPath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/" + styleName + ".qss";
}
QString configPath(const QString &basedir, const QString &styleName) {
  return styleSheetDir(basedir, styleName) + "/config.ini";
}

struct InvalidColorError {
  QString settingsKey;
  QString setting;
};

template <typename T>
void withSettingsValue(const QSettings &settings, const QString &key,
                       std::function<void(const T &)> f) {
  if (settings.contains(key)) {
    QVariant v = settings.value(key);
    if (v.isValid())

      f(v.value<T>());
    else
      qWarning() << QString(
                        "Invalid parameter (%1) for "
                        "setting (%2) in config")
                        .arg(v.toString(), key);
  }
}

void withSettingsColor(const QSettings &settings, const QString &key,
                       std::function<void(const QColor &)> f) {
  if (settings.contains(key)) {
    QString str = settings.value(key).toString();
    if (QColor::isValidColor(str))
      f(QColor(str));
    else
      qWarning() << QString(
                        "Invalid color (%1) for "
                        "setting (%2) in config")
                        .arg(str, key);
  }
}

void setConfigFont(const QSettings &settings, QString &dst,
                   const QString &key) {
  static QStringList valid_families = QFontDatabase().families();
  if (settings.contains(key)) {
    QString str = settings.value(key).toString();
    if (!str.isEmpty() && valid_families.contains(str, Qt::CaseInsensitive)) {
      dst = str;
    } else {
      dst = "Sans serif";
      qWarning() << QString(
                        "Invalid font (%1) for "
                        "setting (%2) in config")
                        .arg(str, key);
    }
  }
}

void setQPaletteColor(QPalette &palette, QPalette::ColorRole role,
                      QSettings &settings, const QString &key) {
  withSettingsColor(settings, key,
                    [&](const QColor &c) { palette.setColor(role, c); });
};

void setConfigColor(const QSettings &settings, QColor &dst,
                    const QString &key) {
  withSettingsColor(settings, key, [&](const QColor &c) { dst = c; });
};

template <typename T>
void setConfigValue(const QSettings &settings, T *dst, const QString &key) {
  withSettingsValue<T>(settings, key, [&](const T &c) { *dst = c; });
};

void loadQPalette(const QString &path, QPalette &palette) {
  QSettings styleConfig(path, QSettings::IniFormat);

  styleConfig.beginGroup("palette");
  setQPaletteColor(palette, QPalette::Window, styleConfig, "Window");
  setQPaletteColor(palette, QPalette::WindowText, styleConfig, "WindowText");
  setQPaletteColor(palette, QPalette::Base, styleConfig, "Base");
  setQPaletteColor(palette, QPalette::ToolTipBase, styleConfig, "ToolTipBase");
  setQPaletteColor(palette, QPalette::ToolTipText, styleConfig, "ToolTipText");
  setQPaletteColor(palette, QPalette::Text, styleConfig, "Text");
  setQPaletteColor(palette, QPalette::Button, styleConfig, "Button");
  setQPaletteColor(palette, QPalette::ButtonText, styleConfig, "ButtonText");
  setQPaletteColor(palette, QPalette::BrightText, styleConfig, "BrightText");
  setQPaletteColor(palette, QPalette::Text, styleConfig, "Text");
  setQPaletteColor(palette, QPalette::Link, styleConfig, "Link");
  setQPaletteColor(palette, QPalette::Highlight, styleConfig, "Highlight");
  setQPaletteColor(palette, QPalette::HighlightedText, styleConfig,
                   "HighlightedText");
  setQPaletteColor(palette, QPalette::Light, styleConfig, "Light");
  setQPaletteColor(palette, QPalette::Dark, styleConfig, "Dark");
  styleConfig.endGroup();
}

void loadConfig(const QString &path, Config &c) {
  QSettings styleConfig(path, QSettings::IniFormat);
  setConfigColor(styleConfig, c.color.MeterBackground, "meter/Background");
  setConfigColor(styleConfig, c.color.MeterBackgroundSoft,
                 "meter/BackgroundSoft");
  setConfigColor(styleConfig, c.color.MeterBar, "meter/Bar");
  setConfigColor(styleConfig, c.color.MeterBarMid, "meter/BarMid");
  setConfigColor(styleConfig, c.color.MeterLabel, "meter/Label");
  setConfigColor(styleConfig, c.color.MeterTick, "meter/Tick");
  setConfigColor(styleConfig, c.color.MeterBarHigh, "meter/BarHigh");

  setConfigColor(styleConfig, c.color.KeyboardBeat, "keyboard/Beat");
  setConfigColor(styleConfig, c.color.KeyboardRootNote, "keyboard/RootNote");
  setConfigColor(styleConfig, c.color.KeyboardWhiteNote, "keyboard/WhiteNote");
  setConfigColor(styleConfig, c.color.KeyboardBlackNote, "keyboard/BlackNote");
  setConfigColor(styleConfig, c.color.KeyboardWhiteLeft, "keyboard/WhiteLeft");
  setConfigColor(styleConfig, c.color.KeyboardBlackLeft, "keyboard/BlackLeft");
  setConfigColor(styleConfig, c.color.KeyboardBlackLeftInner,
                 "keyboard/BlackLeftInner");
  setConfigColor(styleConfig, c.color.KeyboardBlack, "keyboard/Black");
  setConfigColor(styleConfig, c.color.KeyboardMeasure, "keyboard/Measure");

  setConfigColor(styleConfig, c.color.MeasureSeparator,
                 "measure/MeasureSeparator");
  setConfigColor(styleConfig, c.color.MeasureIncluded,
                 "measure/MeasureIncluded");
  setConfigColor(styleConfig, c.color.MeasureExcluded,
                 "measure/MeasureExcluded");
  setConfigColor(styleConfig, c.color.MeasureBeat, "measure/Beat");
  setConfigColor(styleConfig, c.color.MeasureUnitEdit, "measure/UnitEdit");
  setConfigColor(styleConfig, c.color.MeasureNumberBlock,
                 "measure/MeasureNumberBlock");

  setConfigColor(styleConfig, c.color.ParamBlue, "parameters/Blue");
  setConfigColor(styleConfig, c.color.ParamDarkBlue, "parameters/DarkBlue");
  setConfigColor(styleConfig, c.color.ParamDarkTeal, "parameters/DarkTeal");
  setConfigColor(styleConfig, c.color.ParamBrightGreen,
                 "parameters/BrightGreen");
  setConfigColor(styleConfig, c.color.ParamFadedWhite, "parameters/FadedWhite");
  setConfigColor(styleConfig, c.color.ParamFont, "parameters/Font");
  setConfigColor(styleConfig, c.color.ParamBeat, "parameters/Beat");
  setConfigColor(styleConfig, c.color.ParamMeasure, "parameters/Measure");

  setConfigColor(styleConfig, c.color.Playhead, "views/Playhead");
  setConfigColor(styleConfig, c.color.PlayheadRecording,
                 "views/PlayheadRecording");
  setConfigColor(styleConfig, c.color.Cursor, "views/Cursor");

  setConfigColor(styleConfig, c.color.WindowCaption, "platform/WindowCaption");
  setConfigColor(styleConfig, c.color.WindowText, "platform/WindowText");
  setConfigColor(styleConfig, c.color.WindowBorder, "platform/WindowBorder");
  setConfigValue<bool>(styleConfig, &c.other.Win10BorderDark,
                       "platform/WindowDark");

  setConfigFont(styleConfig, c.font.EditorFont, "fonts/Editor");
  setConfigFont(styleConfig, c.font.MeterFont, "fonts/Meter");
}

Config defaultConfig(bool is_system_theme) {
  Config c = Config::empty();
  loadConfig(":/styles/ptCollage/config.ini", c);

  if (is_system_theme) {
    // Use a slightly different colour scheme since the ptcollage style is a bit
    // jarring
    c.color.MeterBackground = qApp->palette().dark().color();
    c.color.MeterBackgroundSoft = qApp->palette().dark().color();
    c.color.MeterBar = Qt::green;
    c.color.MeterBarMid = Qt::yellow;
    c.color.MeterLabel = qApp->palette().text().color();
    c.color.MeterTick = qApp->palette().shadow().color();
    c.color.MeterBarHigh = Qt::red;
  }

  return c;
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

int nonnegative_modulo(int x, int m) {
  if (m == 0) return 0;
  return ((x % m) + m) % m;
}

static std::shared_ptr<QPixmap> currentMeasureImages = nullptr;
const std::shared_ptr<QPixmap> measureImages() { return currentMeasureImages; }

static std::vector<std::shared_ptr<NoteBrush const>> currentNoteBrushes;
std::shared_ptr<NoteBrush const> noteBrush(int i) {
  return currentNoteBrushes[nonnegative_modulo(i, currentNoteBrushes.size())];
}

static Config currentConfig = Config::empty();
const Config &config = currentConfig;

void tryLoadNoteColours(const QString &filename) {
  QImage note_colours(filename);
  if (!note_colours.isNull() && note_colours.width() == 3 &&
      note_colours.height() >= 1) {
    currentNoteBrushes.clear();
    for (int i = 0; i < note_colours.height(); ++i)
      currentNoteBrushes.push_back(std::make_shared<NoteBrush>(
          note_colours.pixel(0, i), note_colours.pixel(1, i),
          note_colours.pixel(2, i)));
  }
}

bool tryLoadStyle(const QString &basedir, const QString &styleName) {
  // A stylesheet needs to exist for any part of the style to be loaded.
  QFile styleSheet = styleSheetPath(basedir, styleName);
  if (!styleSheet.open(QFile::ReadOnly)) {
    qWarning() << "The selected style is not available: " << styleName << " ("
               << styleSheet.fileName() << ")";
    return false;
  }

  loadFonts(styleSheetDir(basedir, styleName));

  Config p = defaultConfig(false);
  QString path = configPath(basedir, styleName);
  if (QFile::exists(path)) {
    QPalette qp(qApp->palette());
    loadQPalette(path, qp);
    qApp->setPalette(qp);
    loadConfig(path, p);
  }
  currentConfig = p;

  qApp->setStyle(QStyleFactory::create("Fusion"));
  // Use Fusion as a base for aspects stylesheet does not cover, it should
  // look consistent across all platforms
  qApp->setStyleSheet(relativizeUrls(styleSheet.readAll(), basedir, styleName));
  styleSheet.close();

  currentMeasureImages = std::make_shared<QPixmap>(":/images/images");
  QPixmap px(styleSheetDir(basedir, styleName) + "/images.png");
  if (!px.isNull() && px.size() == currentMeasureImages->size())
    currentMeasureImages = std::make_shared<QPixmap>(px);

  tryLoadNoteColours(":/images/default_note_colours.png");
  tryLoadNoteColours(styleSheetDir(basedir, styleName) + "/note_colours.png");

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
    currentConfig = defaultConfig(true);
    currentMeasureImages = std::make_shared<QPixmap>(":/images/images");
    tryLoadNoteColours(":/images/default_note_colours.png");
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

bool setWindowBorderColor(QWidget *w) {
#ifdef Q_OS_WINDOWS

  if (QOperatingSystemVersion::current() >=
      QOperatingSystemVersion::Windows10) {
    static ULONG osBuildNumber = *reinterpret_cast<DWORD *>(0x7FFE0000 + 0x260);
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/ns-ntddk-kuser_shared_data#syntax
    // member "NtMinorVersion" of _KUSER_SHARED_DATA. don't even ask where the
    // address comes from

    //  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
    static auto hdwmapi = LoadLibraryW(L"dwmapi.dll");
    if (hdwmapi) {
      auto qColorToColorRef = [](const QColor &c) -> COLORREF {
        return (c.red() | c.green() << 8 | c.blue() << 16);
      };

      quint32 darkTitleBar = StyleEditor::config.other.Win10BorderDark;
      typedef int(WINAPI * DWMSETWINDOWATTRIBUTE)(
          HWND hwnd, DWORD dwAttribute, LPCVOID pvAttribute, DWORD cbAttribute);
      static auto DwmSetWindowAttribute =
          reinterpret_cast<DWMSETWINDOWATTRIBUTE>(
              GetProcAddress(hdwmapi, "DwmSetWindowAttribute"));
      auto hwnd = (HWND)w->winId();
      DwmSetWindowAttribute(
          hwnd,
          (osBuildNumber >= 18985)
              ? 20
              : 19 /*DWMWINDOWATTRIBUTE:DWMWA_USE_IMMERSIVE_DARK_MODE*/,
          &darkTitleBar, sizeof(uint32_t));

      qDebug() << osBuildNumber;
      if (osBuildNumber >= 22000) {
        QColor border = StyleEditor::config.color.WindowBorder;
        if (border.isValid()) {
          COLORREF b = qColorToColorRef(border);
          DwmSetWindowAttribute(hwnd,
                                34 /*DWMWINDOWATTRIBUTE::DWMWA_BORDER_COLOR*/,
                                &b, sizeof(b));
        }

        QColor caption = StyleEditor::config.color.WindowCaption;
        if (caption.isValid()) {
          COLORREF c = qColorToColorRef(caption);
          DwmSetWindowAttribute(hwnd,
                                35 /*DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR*/,
                                &c, sizeof(c));
        }
        QColor text = StyleEditor::config.color.WindowText;
        if (text.isValid()) {
          COLORREF t = qColorToColorRef(text);
          DwmSetWindowAttribute(
              hwnd, 36 /*DWMWINDOWATTRIBUTE::DWMWA_TEXT_COLOR*/, &t, sizeof(t));
        }
      }
    }
  }

#elif Q_OS_OSX
  // Soon
#endif
  return 1;
}
}  // namespace StyleEditor
