#include "StyleEditor.h"

#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
#include <QObject>
#include <QPalette>
#include <QStyleFactory>

#include "Settings.h"

namespace StyleEditor {

QString styleSheetDir(const QString &styleName) {
  return qApp->applicationDirPath() + "/style/" + styleName;
}
QString styleSheetPath(const QString &styleName) {
  return styleSheetDir(styleName) + "/" + styleName + ".qss";
}

struct InvalidColorError {
  QString settingsKey;
  QString setting;
};
void setColorFromSetting(QPalette &palette, QPalette::ColorRole role,
                         QSettings &settings, const QString &key) {
  QString str = settings.value(key).toString();
  if (QColor::isValidColor(str))
    palette.setColor(role, str);
  else
    throw InvalidColorError{key, str};
};

void tryLoadPalette(const QString &styleName) {
  QString path = styleSheetDir(styleName) + "/palette.ini";
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
      qApp->setPalette(palette);
    } catch (InvalidColorError e) {
      qWarning()
          << QString(
                 "Could not load palette. Invalid color (%1) for setting (%2)")
                 .arg(e.setting, e.settingsKey);
    }
  }
}

bool tryLoadStyle(const QString &styleName) {
  if (styleName == "System") return true;

  QFile styleSheet = styleSheetPath(styleName);
  if (!styleSheet.open(QFile::ReadOnly)) {
    qWarning() << "The selected style is not available:"
               << Settings::StyleName::get();
    return false;
  }

  tryLoadPalette(styleName);
  // Only apply custom palette if palette.ini is present. For minimal
  // sheets, the availability of a palette helps their changes blend
  // in with unchanged aspects.
  qApp->setStyle(QStyleFactory::create("Fusion"));
  // Use Fusion as a base for aspects stylesheet does not cover, it should
  // look consistent across all platforms
  qApp->setStyleSheet(styleSheet.readAll());
  styleSheet.close();
  return true;
}

QStringList getStyles() {
  QStringList styles;
  QString targetFilename;
  styles.push_front("System");
  QDirIterator dir(qApp->applicationDirPath() + "/style",
                   QDirIterator::NoIteratorFlags);
  while (dir.hasNext()) {
    dir.next();
    if (!dir.fileName().isEmpty() && dir.fileName() != "." &&
        dir.fileName() != "..") {
      targetFilename = styleSheetPath(dir.fileName());
      if (QFile(targetFilename).exists()) {
        styles.push_front(dir.fileName());
      }
    }
  }  // Search for directories that have QSS files of the same name in them,
  return styles;
}
}  // namespace StyleEditor
