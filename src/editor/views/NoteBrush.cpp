#include "NoteBrush.h"

NoteBrush::NoteBrush(const QColor &zero, const QColor &base, const QColor &max)
    : zero(zero), base(base), max(max){};

NoteBrush::NoteBrush(int hue) {
  int muted_brightness = 96;
  int base_brightness = 204;
  int on_brightness = 255;
  int muted_saturation = 48;
  int base_saturation = 255;
  zero = QColor::fromHsl(hue, muted_brightness, muted_saturation);
  base = QColor::fromHsl(hue, base_brightness, base_saturation);
  max = QColor::fromHsl(hue, on_brightness, base_saturation);
}

QColor lerpColor(double r, const QColor &a, const QColor &b) {
  return QColor::fromRgb(lerp(r, a.red(), b.red()),
                         lerp(r, a.green(), b.green()),
                         lerp(r, a.blue(), b.blue()));
}

QColor NoteBrush::toQColor(int velocity, double on_strength, int alpha) const {
  double velocity_strength = double(velocity) / EVENTMAX_VELOCITY;
  QColor c =
      lerpColor(velocity_strength, zero, lerpColor(on_strength, base, max));
  c.setAlpha(alpha);
  return c;
}
