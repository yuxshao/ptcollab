#include "StyleEditor.h"

#include <QApplication>
#include <QDirIterator>
#include <QFile>
#include <QMessageBox>
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

void determineStyle() {
  QString styleName = Settings::StyleName::get();
  if (styleName == "System") return;
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

void interpretStyle() {
  determineStyle();

  if (Settings::StyleName::get() != "System") {
    QString styleSheetName = Settings::StyleName::get();
    QFile styleSheet = qApp->applicationDirPath() + "/style/" + styleSheetName +
                       "/" + styleSheetName + ".qss";
    styleSheet.open(QFile::ReadOnly);
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
    styleSheet.close();
  }
}

QStringList getStyles() {
  QStringList styles;
  QString targetFilename;
  styles.push_front("System");
  QDirIterator dir(qApp->applicationDirPath() + "/style",
                   QDirIterator::NoIteratorFlags);
  while (dir.hasNext()) {
    if (!dir.fileName().isEmpty() && dir.fileName() != "." &&
        dir.fileName() != "..") {
      targetFilename = dir.filePath() + "/" + dir.fileName() + ".qss";
      if (QFile(targetFilename).exists()) {
        styles.push_front(dir.fileName());
      }
    }
    dir.next();
  }  // Search for directories that have QSS files of the same name in them,
  return styles;
}

QString getStyle() { return Settings::StyleName::get(); }
}  // namespace StyleEditor
