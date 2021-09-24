#include "IconHelper.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>

void recolor(QPainter *painter, const QPixmap &src, QColor color) {
  QImage srcImage = src.toImage();
  const auto format = srcImage.hasAlphaChannel()
                          ? QImage::Format_ARGB32_Premultiplied
                          : QImage::Format_RGB32;
  srcImage = std::move(srcImage).convertToFormat(format);

  QRect rect(0, 0, src.width(), src.height());
  QImage destImage = QImage(rect.size(), srcImage.format());
  destImage.setDevicePixelRatio(src.devicePixelRatioF());

  // do colorizing
  QPainter destPainter(&destImage);
  destPainter.fillRect(rect, color);
  destPainter.end();
  destImage.setAlphaChannel(srcImage.convertToFormat(QImage::Format_Alpha8));
  painter->drawImage(QPoint(0, 0), destImage);
}
// Adapted from
// https://code.woboq.org/qt5/qtbase/src/widgets/effects/qpixmapfilter.cpp.html#1089
// (Simplified for whole image, strength 1)

QIcon getIcon(QString name) {
  QPixmap px = QIcon(":/icons/neutral/" + name).pixmap(128, 128);
  QPainter p(&px);
  recolor(&p, px, qApp->palette().color(QPalette::ButtonText));
  return QIcon(px);
}
