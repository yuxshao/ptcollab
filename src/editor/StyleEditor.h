#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPixmap>
#include <QStringList>

#include "views/NoteBrush.h"
#include <QWindow>
#include <QOperatingSystemVersion>
#include <QString>
#include <QLayout>

#if defined(Q_OS_WINDOWS)
#define NOMINMAX
#include <Windows.h>
#endif

#if defined(Q_OS_WINDOWS) || defined(Q_OS_MACOS)
void customizeNativeTitleBar(WId w) noexcept;
// On Windows, defined in StyleEditor.cpp
// On macOS, defined in MacOsStyleEditor.mm
#endif

namespace StyleEditor {
void initializeStyleDir();
void setTitleBar(QWidget *w) noexcept;
bool tryLoadStyle(const QString &styleName);
const std::shared_ptr<QPixmap> measureImages();
std::shared_ptr<NoteBrush const> noteBrush(int i);

class EventFilter : public QObject {
  Q_OBJECT
 public:
  bool eventFilter(QObject *watched, QEvent *event) {
    switch (event->type()) {
      case QEvent::Show:
        // https://doc.qt.io/qt-5/qshowevent.html
        // Spontaneous QShowEvents are sent by the window system after the
        // window is shown, as well as when it's reshown after being iconified.
        // We only want this to trigger when Qt sends it, which is right before
        // it becomes visible.
        if (!event->spontaneous()) {
          QWidget *w = qobject_cast<QWidget *>(watched);
          if (w) setTitleBar(w);
        }

        return false;
      default:
        return false;
    }
  }
};

struct Config {
 private:
  Config() {}
  // Constructor is private so I don't accidentally do
  // StyleEditor::Palette() instead of ::palette

 public:
  struct color {
    QColor MeterBackground;
    QColor MeterBackgroundSoft;
    QColor MeterBar;
    QColor MeterBarMid;
    QColor MeterLabel;
    QColor MeterTick;
    QColor MeterBarHigh;

    QColor KeyboardBeat;
    QColor KeyboardRootNote;
    QColor KeyboardWhiteNote;
    QColor KeyboardBlackNote;
    QColor KeyboardWhiteLeft;
    QColor KeyboardBlackLeft;
    QColor KeyboardBlackLeftInner;
    QColor KeyboardBlack;
    QColor KeyboardMeasure;

    QColor MeasureSeparator;
    QColor MeasureIncluded;
    QColor MeasureExcluded;
    QColor MeasureBeat;
    QColor MeasureUnitEdit;
    QColor MeasureNumberBlock;

    QColor ParamBlue;
    QColor ParamDarkBlue;
    QColor ParamDarkTeal;
    QColor ParamBrightGreen;
    QColor ParamFadedWhite;
    QColor ParamFont;
    QColor ParamBeat;
    QColor ParamMeasure;

    QColor Playhead;
    QColor PlayheadRecording;
    QColor Cursor;

    QColor WindowCaption;
    QColor WindowText;
    QColor WindowBorder;
  } color;

  struct font {
    QString EditorFont;
    QString MeterFont;
  } font;

  struct other {
    bool Win10BorderDark = false; //bool
  } other;

  static Config empty() { return {}; }
};

extern const Config &config;
QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
