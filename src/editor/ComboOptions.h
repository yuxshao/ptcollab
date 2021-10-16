#ifndef COMBOOPTIONS_H
#define COMBOOPTIONS_H

#include <QString>
#include <vector>

#include "pxtone/pxtnEvelist.h"
extern const std::vector<std::pair<QString, int>> &quantizeXOptions();
extern const std::vector<std::pair<QString, int>> &quantizeYOptionsSimple();
extern const std::vector<std::pair<QString, int>> &quantizeYOptionsAdvanced();
extern const std::vector<std::pair<QString, int>> &keyboardDisplayOptions();
extern const std::vector<std::pair<QString, EVENTKIND>> &paramOptions();

#endif  // COMBOOPTIONS_H
