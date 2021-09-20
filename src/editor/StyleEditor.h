#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

#include <QStringList>

namespace StyleEditor {
void initializeStyleDir();
bool tryLoadStyle(const QString &styleName);
QStringList getStyles();
} // namespace StyleEditor

#endif // STYLEEDITOR_H
