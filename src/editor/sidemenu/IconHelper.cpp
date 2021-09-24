#include "IconHelper.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>

static void grayscale(const QImage &image, QImage &dest) {
  const unsigned int *data = (const unsigned int *)image.bits();
  unsigned int *outData = (unsigned int *)dest.bits();
  // a bit faster loop for grayscaling everything
  int pixels = dest.width() * dest.height();
  for (int i = 0; i < pixels; ++i) {
    int val = qGray(data[i]);
    outData[i] = qRgba(val, val, val, qAlpha(data[i]));
  }
}
// This is reimplemented qpixmapfilter.cpp behavior.
// https://code.woboq.org/qt5/qtbase/src/widgets/effects/qpixmapfilter.cpp.html#946

void recolor(QPainter *painter, const QPixmap &src, QColor color,
             unsigned int strength) {
  if (src.isNull()) return;
  // raster implementation
  QImage srcImage;
  QImage destImage;
  QRectF srcRect(0, 0, src.width(), src.height());
  QRect rect = srcRect.toAlignedRect().intersected(src.rect());
  srcImage = src.copy(rect).toImage();
  const auto format = srcImage.hasAlphaChannel()
                          ? QImage::Format_ARGB32_Premultiplied
                          : QImage::Format_RGB32;
  srcImage = std::move(srcImage).convertToFormat(format);
  destImage = QImage(rect.size(), srcImage.format());

  destImage.setDevicePixelRatio(src.devicePixelRatioF());
  // do colorizing
  QPainter destPainter(&destImage);
  grayscale(srcImage, destImage);
  destPainter.setCompositionMode(QPainter::CompositionMode_Screen);
  destPainter.fillRect(srcImage.rect(), color);
  destPainter.end();
  // alpha blending srcImage and destImage
  QImage buffer = srcImage;
  QPainter bufPainter(&buffer);
  bufPainter.setOpacity(strength);
  bufPainter.drawImage(0, 0, destImage);
  bufPainter.end();
  destImage = std::move(buffer);
  //  if (srcImage.hasAlphaChannel())
  destImage.setAlphaChannel(srcImage.convertToFormat(QImage::Format_Alpha8));
  painter->drawImage(QPoint(0, 0), destImage);
}
// This is reimplemented  QPixmapColorizeFilter behavior.
// https://code.woboq.org/qt5/qtbase/src/widgets/effects/qpixmapfilter.cpp.html#1089

QIcon getIcon(QString name) {
  QPixmap px = QIcon(":/icons/neutral/" + name).pixmap(128, 128);
  QPainter p(&px);
  recolor(&p, px, qApp->palette().color(QPalette::ButtonText), 1);
  return QIcon(px);
}
