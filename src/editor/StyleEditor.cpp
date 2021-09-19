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
    palette.setColor(QPalette::Window, str);
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

void interpretStyle() {
  QString styleName = Settings::StyleName::get();
  if (styleName != "System") {
    if (styleName.isEmpty() || !QFile::exists(styleSheetPath(styleName))) {
      QFile requestedDefaultStyle = styleSheetPath(requestedDefaultStyleName);
      if (requestedDefaultStyle.exists()) {
        Settings::StyleName::set(requestedDefaultStyleName);
      } else {
        Settings::StyleName::set("System");
      }
    }
  }
  // Set default style if current style is not
  // present or unset (meaning files were moved between now and the last start
  // or settings are unset/previously cleared). In that case set default style
  // to ptCollage in every instance possible unless erreneous or absent (then
  // it's "System").

  if (Settings::StyleName::get() != "System") {
    QString styleName = Settings::StyleName::get();
    QFile styleSheet = styleSheetPath(styleName);
    if (styleSheet.open(QFile::ReadOnly)) {
      loadPaletteIfExists(styleName);
      // Only apply custom palette if palette.ini is present. QPalette::Light
      // and QPalette::Dark have been discarded because they are unlikely to be
      // used in conjunction with stylesheets & they cannot be represented
      // properly in an .INI. There are situations where the palette would not
      // be a necessary addition to the stylesheet & all the coloring is done
      // first-hand by the stylesheet author. For minimal sheets, though, the
      // availability of a palette helps their changes blend in with unchanged
      // aspects.
      qApp->setStyle(QStyleFactory::create("Fusion"));
      // Use Fusion as a base for aspects stylesheet does not cover, it should
      // look consistent across all platforms
      qApp->setStyleSheet(styleSheet.readAll());
      styleSheet.close();
    } else {
      QMessageBox::critical(
          nullptr, QObject::tr("Style Loading Error"),
          QObject::tr("The selected style (%1) has errors. The "
                      "default style will be used.")
              .arg(Settings::StyleName::get()));
      if (Settings::StyleName::get() == requestedDefaultStyleName)
        Settings::StyleName::set("System");
      // This should only happen if the stylesheet is unreadable
    }
  }
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

QString getStyle() { return Settings::StyleName::get(); }
void setStyle(const QString style) { Settings::StyleName::set(style); }
}  // namespace StyleEditor
