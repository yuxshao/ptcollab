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

#ifdef Q_OS_WINDOWS
#include <Windows.h>
#elif Q_OS_OSX
  // Soon
#endif

namespace StyleEditor {
void initializeStyleDir();
bool tryLoadStyle(const QString &styleName);
const std::shared_ptr<QPixmap> measureImages();
bool setWindowBorderColor(QWidget *w);
std::shared_ptr<NoteBrush const> noteBrush(int i);
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
    bool Win10BorderDark; //bool
  } other;

  static Config empty() { return {}; }
};

extern const Config &config;
QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
