#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QColor>
#include <QPixmap>
#include <QStringList>

namespace StyleEditor {
void initializeStyleDir();
bool tryLoadStyle(const QString &styleName);
const std::shared_ptr<QPixmap> measureImages();
struct Palette {
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
  QColor Cursor;
};
const Palette &palette();

QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
