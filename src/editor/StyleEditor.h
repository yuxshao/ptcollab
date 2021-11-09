#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QColor>
#include <QPixmap>
#include <QStringList>

namespace StyleEditor {
void initializeStyleDir();
bool tryLoadStyle(const QString &styleName);
const QPixmap getMeasureImages();
QHash<QString, QColor> tryLoadMeterPalette();
QHash<QString, QColor> tryLoadKeyboardPalette();
QHash<QString, QColor> tryLoadMeasurePalette();
QHash<QString, QColor> tryLoadParametersPalette();
QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
