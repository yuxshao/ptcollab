#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QColor>
#include <QPixmap>
#include <QStringList>

namespace StyleEditor {
void initializeStyleDir();
void setSystemStyle();
bool tryLoadStyle(const QString &styleName);
const QPixmap getMeasureImages();
QColor getCommonViewColor(QString key);
QHash<QString, QColor> getMeterPalette();
QHash<QString, QColor> getKeyboardPalette();
QHash<QString, QColor> getMeasurePalette();
QHash<QString, QColor> getParametersPalette();
QStringList getStyles();
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
