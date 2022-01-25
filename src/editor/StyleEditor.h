#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QPixmap>
#include <QStringList>

#include "Settings.h"

namespace StyleEditor {
void initializeStyleDir();
bool tryLoadStyle(const QString &styleName);
const std::shared_ptr<QPixmap> measureImages();
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
  } color;

  struct font {
    QString EditorFont;
    QString MeterFont;
  } font;

  static Config empty() { return {}; }
};

extern const Config &config;
QColor getPlayheadColor();
QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
