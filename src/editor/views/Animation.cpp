#include "Animation.h"

Animation::Animation(QObject *parent) : QVariantAnimation(parent) {
  setDuration(100);
  setStartValue(0);
  setEndValue(360);
  setEasingCurve(QEasingCurve::Linear);
  setLoopCount(-1);
  start();

  connect(this, &QVariantAnimation::valueChanged, this, &Animation::nextFrame);
}
