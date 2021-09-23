#include "IconHelper.h"

#include <QApplication>
#include <QGraphicsColorizeEffect>
#include <QPainter>
#include <QPalette>

#include <QtDebug>

static void grayscale(const QImage &image, QImage &dest,
                      const QRect &rect = QRect()) {
  QRect destRect = rect;
  QRect srcRect = rect;
  if (rect.isNull()) {
    srcRect = dest.rect();
    destRect = dest.rect();
  }
  if (&image != &dest) {
    destRect.moveTo(QPoint(0, 0));
  }
  const unsigned int *data = (const unsigned int *)image.bits();
  unsigned int *outData = (unsigned int *)dest.bits();
  if (dest.size() == image.size() && image.rect() == srcRect) {
    // a bit faster loop for grayscaling everything
    int pixels = dest.width() * dest.height();
    for (int i = 0; i < pixels; ++i) {
      int val = qGray(data[i]);
      outData[i] = qRgba(val, val, val, qAlpha(data[i]));
    }
  } else {
    int yd = destRect.top();
    for (int y = srcRect.top(); y <= srcRect.bottom() && y < image.height();
         y++) {
      data = (const unsigned int *)image.scanLine(y);
      outData = (unsigned int *)dest.scanLine(yd++);
      int xd = destRect.left();
      for (int x = srcRect.left(); x <= srcRect.right() && x < image.width();
           x++) {
        int val = qGray(data[x]);
        outData[xd++] = qRgba(val, val, val, qAlpha(data[x]));
      }
    }
  }
}
// This is reimplemented qpixmapfilter.cpp behavior.
// https://code.woboq.org/qt5/qtbase/src/widgets/effects/qpixmapfilter.cpp.html#946

void recolor(QPainter *painter, const QPixmap &src, QColor color,
             unsigned int strength) {
  if (src.isNull())
    return;
  // raster implementation
  QImage srcImage;
  QImage destImage;
  QRectF srcRect(0, 0, src.width(), src.height());
  if (srcRect.isNull()) {
    srcImage = src.toImage();
    const auto format = srcImage.hasAlphaChannel()
                            ? QImage::Format_ARGB32_Premultiplied
                            : QImage::Format_RGB32;
    srcImage = std::move(srcImage).convertToFormat(format);
    destImage = QImage(srcImage.size(), srcImage.format());
  } else {
    QRect rect = srcRect.toAlignedRect().intersected(src.rect());
    srcImage = src.copy(rect).toImage();
    const auto format = srcImage.hasAlphaChannel()
                            ? QImage::Format_ARGB32_Premultiplied
                            : QImage::Format_RGB32;
    srcImage = std::move(srcImage).convertToFormat(format);
    destImage = QImage(rect.size(), srcImage.format());
  }
  destImage.setDevicePixelRatio(src.devicePixelRatioF());
  // do colorizing
  QPainter destPainter(&destImage);
  grayscale(srcImage, destImage, srcImage.rect());
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
  destImage.setAlphaChannel(srcImage.alphaChannel());
  painter->drawImage(QPoint(0, 0), destImage);
}
// This is reimplemented  QPixmapColorizeFilter behavior.
// https://code.woboq.org/qt5/qtbase/src/widgets/effects/qpixmapfilter.cpp.html#1089

QIcon getIcon(QString name) {
  QPixmap px = QIcon(":/icons/neutral/" + name).pixmap(32, 32);
  QPainter p(&px);

  qDebug() << qApp->palette().color(QPalette::ButtonText).name();
  recolor(&p, px, qApp->palette().color(QPalette::ButtonText), 1);

  return QIcon(px);
}
