#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QStringList>

namespace StyleEditor {
void determineStyle();
void interpretStyle();
QStringList getStyles();
QString getStyle();
void setStyle(const QString style);
}  // namespace StyleEditor

#endif  // STYLEEDITOR_H
