#ifndef NOTEBRUSH_H
#define NOTEBRUSH_H

#include <QColor>

#include "ViewHelper.h"

constexpr int EVENTMAX_VELOCITY = 128;
class NoteBrush {
  QColor zero;
  QColor base;
  QColor max;

 public:
  NoteBrush(const QColor &zero, const QColor &base, const QColor &max);
  NoteBrush(int hue);
  QColor toQColor(int velocity, double on_strength, int alpha) const;
};

extern const NoteBrush default_note_brushes[];
extern const int DEFAULT_NUM_NOTE_BRUSHES;

#endif  // NOTEBRUSH_H
