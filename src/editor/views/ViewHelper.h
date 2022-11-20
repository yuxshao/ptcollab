#ifndef VIEWHELPER_H
#define VIEWHELPER_H

#include <QColor>
#include <QPainter>
#include <QPainterPath>

#include "editor/PxtoneClient.h"

extern int impliedVelocity(MouseEditState state, qreal pitchPerPx);
extern void drawCursor(const QPoint &position, QPainter &painter,
                       const QColor &color, const QString &username,
                       qint64 uid);
extern void drawCurrentPlayerPosition(QPainter &painter, int now, int height,
                                      qreal clockPerPx, bool drawHead);
extern void drawPlayhead(QPainter &painter, qint32 x, qint32 height,
                         QColor color, bool drawHead);
extern void drawLastSeek(QPainter &painter, const PxtoneClient *client,
                         qint32 height, bool drawHead);
extern void drawRepeatAndEndBars(QPainter &painter, const pxtnMaster *master,
                                 qreal clockPerPx, int height);
extern void handleWheelEventWithModifier(QWheelEvent *event,
                                         PxtoneClient *client);

template <typename T>
T clamp(T x, T lo, T hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

template <typename T>
T distance_to_range(T x, T lo, T hi) {
  if (x < lo) return lo - x;
  if (x > hi) return x - hi;
  return 0;
}

extern int lerp(double r, int a, int b);
extern double lerp_f(double r, double a, double b);

extern int nonnegative_modulo(int x, int m);
extern int one_over_last_clock(pxtnService const *pxtn);
extern void drawSelection(QPainter &painter, const Interval &interval,
                          qint32 height, double alphaMultiplier);
extern void drawExistingSelection(QPainter &painter,
                                  const MouseEditState &state, qreal clockPerPx,
                                  qint32 height, double alphaMultiplier);
extern const int WINDOW_BOUND_SLACK;
extern void fillUnitBullet(QPainter &painter, int thisX, int y, int w,
                           const QColor &color);
extern void drawUnitBullet(QPainter &painter, int thisX, int y, int w);

extern void drawNumAlignTopRight(QPainter *painter, int x, int y, int num);
extern void drawOctaveNumAlignBottomLeft(QPainter *painter, int x, int y,
                                         int num, int height, bool a);
extern QTransform worldTransform();

#endif  // VIEWHELPER_H
